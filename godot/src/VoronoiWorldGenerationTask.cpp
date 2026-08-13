#include "VoronoiWorldGenerationTask.h"

#include "WorldGenerator.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/main_loop.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

namespace worldgen {

struct AsyncWorldGenerationState {
    std::mutex mutex;
    std::unique_ptr<World> world;
    std::string errorMessage;
};

namespace {

struct WorkerRequest {
    WorkerRequest(WorldGenerationSettings settings,
                  std::shared_ptr<AsyncWorldGenerationState> state)
        : generator(std::move(settings)),
          state(std::move(state)) {}

    WorldGenerator generator;
    std::shared_ptr<AsyncWorldGenerationState> state;
};

constexpr auto invalidTaskId =
    static_cast<std::int64_t>(godot::WorkerThreadPool::INVALID_TASK_ID);

} // namespace

void VoronoiWorldGenerationTask::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("get_status"),
                                &VoronoiWorldGenerationTask::status);
    godot::ClassDB::bind_method(godot::D_METHOD("is_finished"),
                                &VoronoiWorldGenerationTask::isFinished);
    godot::ClassDB::bind_method(godot::D_METHOD("is_successful"),
                                &VoronoiWorldGenerationTask::isSuccessful);
    godot::ClassDB::bind_method(godot::D_METHOD("get_result"),
                                &VoronoiWorldGenerationTask::result);
    godot::ClassDB::bind_method(godot::D_METHOD("get_error_message"),
                                &VoronoiWorldGenerationTask::errorMessage);
    godot::ClassDB::bind_method(godot::D_METHOD("_poll_async_generation"),
                                &VoronoiWorldGenerationTask::poll);
    godot::ClassDB::bind_method(
        godot::D_METHOD("_finish_deferred_failure", "message"),
        &VoronoiWorldGenerationTask::finishDeferredFailure);

    ADD_SIGNAL(godot::MethodInfo(
        "completed",
        godot::PropertyInfo(godot::Variant::OBJECT,
                            "result",
                            godot::PROPERTY_HINT_RESOURCE_TYPE,
                            "VoronoiWorldData")));
    ADD_SIGNAL(godot::MethodInfo(
        "failed",
        godot::PropertyInfo(godot::Variant::STRING, "error_message")));
    ADD_SIGNAL(godot::MethodInfo("finished"));

    BIND_CONSTANT(STATUS_PENDING);
    BIND_CONSTANT(STATUS_RUNNING);
    BIND_CONSTANT(STATUS_SUCCEEDED);
    BIND_CONSTANT(STATUS_FAILED);

    constexpr auto readOnly = godot::PROPERTY_USAGE_DEFAULT
                              | godot::PROPERTY_USAGE_READ_ONLY;
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT,
                                     "status",
                                     godot::PROPERTY_HINT_ENUM,
                                     "Pending,Running,Succeeded,Failed",
                                     readOnly),
                 "", "get_status");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::BOOL,
                                     "done",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "is_finished");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::BOOL,
                                     "successful",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "is_successful");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT,
                                     "result",
                                     godot::PROPERTY_HINT_RESOURCE_TYPE,
                                     "VoronoiWorldData",
                                     readOnly),
                 "", "get_result");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING,
                                     "error_message",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_error_message");
}

VoronoiWorldGenerationTask::VoronoiWorldGenerationTask()
    : m_state(std::make_shared<AsyncWorldGenerationState>()) {}

std::int64_t VoronoiWorldGenerationTask::status() const noexcept {
    return m_status;
}

bool VoronoiWorldGenerationTask::isFinished() const noexcept {
    return m_status == STATUS_SUCCEEDED || m_status == STATUS_FAILED;
}

bool VoronoiWorldGenerationTask::isSuccessful() const noexcept {
    return m_status == STATUS_SUCCEEDED;
}

godot::Ref<VoronoiWorldData> VoronoiWorldGenerationTask::result() const {
    return m_result;
}

godot::String VoronoiWorldGenerationTask::errorMessage() const {
    return m_errorMessage;
}

void VoronoiWorldGenerationTask::start(WorldGenerationSettings settings) {
    if (m_status != STATUS_PENDING || m_workerTaskId != invalidTaskId)
        throw std::logic_error(
            "An asynchronous generation task can only be started once.");

    auto request = std::make_unique<WorkerRequest>(
        std::move(settings), m_state);

    auto *workerPool = godot::WorkerThreadPool::get_singleton();
    if (workerPool == nullptr)
        throw std::runtime_error("Godot's worker thread pool is unavailable.");

    auto *engine = godot::Engine::get_singleton();
    auto *mainLoop = engine != nullptr ? engine->get_main_loop() : nullptr;
    if (mainLoop == nullptr || !mainLoop->has_signal("process_frame")) {
        throw std::runtime_error(
            "Asynchronous world generation requires an active SceneTree main loop.");
    }

    m_pollCallable = godot::Callable{this, "_poll_async_generation"};
    const auto connectionError = mainLoop->connect(
        "process_frame", m_pollCallable);
    if (connectionError != godot::OK) {
        m_pollCallable = {};
        throw std::runtime_error(
            "Failed to register asynchronous world generation polling.");
    }

    m_mainLoopId = mainLoop->get_instance_id();
    m_keepAlive.reference_ptr(this);
    m_status = STATUS_RUNNING;
    m_workerTaskId = workerPool->add_native_task(
        &VoronoiWorldGenerationTask::generateOnWorker,
        request.get(),
        false,
        "Generate Voronoi world");
    if (m_workerTaskId == invalidTaskId) {
        disconnectPoll();
        m_status = STATUS_PENDING;
        m_keepAlive.unref();
        throw std::runtime_error(
            "Failed to submit world generation to Godot's worker pool.");
    }

    request.release();
}

void VoronoiWorldGenerationTask::failDeferred(
    const godot::String &message) {
    if (m_status != STATUS_PENDING)
        return;

    m_keepAlive.reference_ptr(this);
    call_deferred("_finish_deferred_failure", message);
}

void VoronoiWorldGenerationTask::generateOnWorker(void *userData) {
    const std::unique_ptr<WorkerRequest> request{
        static_cast<WorkerRequest *>(userData),
    };

    try {
        auto world = std::make_unique<World>(request->generator.generate());
        const std::scoped_lock lock{request->state->mutex};
        request->state->world = std::move(world);
    } catch (const std::exception &error) {
        const std::scoped_lock lock{request->state->mutex};
        request->state->errorMessage = error.what();
    } catch (...) {
        const std::scoped_lock lock{request->state->mutex};
        request->state->errorMessage =
            "World generation failed with an unknown native exception.";
    }
}

void VoronoiWorldGenerationTask::poll() {
    if (m_status != STATUS_RUNNING || m_workerTaskId == invalidTaskId)
        return;

    auto *workerPool = godot::WorkerThreadPool::get_singleton();
    if (workerPool == nullptr) {
        finishFailure("Godot's worker thread pool became unavailable.");
        return;
    }
    if (!workerPool->is_task_completed(m_workerTaskId))
        return;

    const auto waitError = workerPool->wait_for_task_completion(m_workerTaskId);
    m_workerTaskId = invalidTaskId;
    disconnectPoll();
    if (waitError != godot::OK) {
        finishFailure("Godot failed to collect the completed world generation task.");
        return;
    }

    std::unique_ptr<World> world;
    std::string workerError;
    {
        const std::scoped_lock lock{m_state->mutex};
        world = std::move(m_state->world);
        workerError = std::move(m_state->errorMessage);
    }
    m_state.reset();

    if (!workerError.empty()) {
        finishFailure(godot::String{workerError.c_str()});
        return;
    }
    if (world == nullptr) {
        finishFailure("World generation completed without returning data.");
        return;
    }

    try {
        godot::Ref<VoronoiWorldData> generatedWorld;
        generatedWorld.instantiate();
        generatedWorld->populate(*world);
        m_result = generatedWorld;
        m_status = STATUS_SUCCEEDED;

        const godot::Ref<VoronoiWorldGenerationTask> keepAlive = m_keepAlive;
        emit_signal("completed", m_result);
        emit_signal("finished");
        m_keepAlive.unref();
    } catch (const std::exception &error) {
        finishFailure(godot::String{error.what()});
    }
}

void VoronoiWorldGenerationTask::finishDeferredFailure(
    const godot::String &message) {
    if (m_status != STATUS_PENDING)
        return;
    finishFailure(message);
}

void VoronoiWorldGenerationTask::finishFailure(
    const godot::String &message) {
    if (isFinished())
        return;

    disconnectPoll();
    m_errorMessage = message;
    m_status = STATUS_FAILED;

    const godot::Ref<VoronoiWorldGenerationTask> keepAlive = m_keepAlive;
    emit_signal("failed", m_errorMessage);
    emit_signal("finished");
    m_keepAlive.unref();
}

void VoronoiWorldGenerationTask::disconnectPoll() {
    if (m_mainLoopId != 0 && m_pollCallable.is_valid()) {
        auto *mainLoop = godot::ObjectDB::get_instance(m_mainLoopId);
        if (mainLoop != nullptr
            && mainLoop->is_connected("process_frame", m_pollCallable)) {
            mainLoop->disconnect("process_frame", m_pollCallable);
        }
    }
    m_mainLoopId = 0;
    m_pollCallable = {};
}

} // namespace worldgen
