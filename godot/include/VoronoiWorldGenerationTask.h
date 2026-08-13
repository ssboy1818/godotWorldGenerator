#pragma once

#include "VoronoiWorldData.h"

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstdint>
#include <memory>

namespace worldgen {

struct AsyncWorldGenerationState;
struct WorldGenerationSettings;
class VoronoiWorldGenerator;

class VoronoiWorldGenerationTask final : public godot::RefCounted {
    GDCLASS(VoronoiWorldGenerationTask, godot::RefCounted)

public:
    enum {
        STATUS_PENDING = 0,
        STATUS_RUNNING = 1,
        STATUS_SUCCEEDED = 2,
        STATUS_FAILED = 3,
    };

    VoronoiWorldGenerationTask();

    [[nodiscard]] std::int64_t status() const noexcept;
    [[nodiscard]] bool isFinished() const noexcept;
    [[nodiscard]] bool isSuccessful() const noexcept;
    [[nodiscard]] godot::Ref<VoronoiWorldData> result() const;
    [[nodiscard]] godot::String errorMessage() const;

protected:
    static void _bind_methods();

private:
    std::shared_ptr<AsyncWorldGenerationState> m_state;
    godot::Ref<VoronoiWorldGenerationTask> m_keepAlive;
    godot::Ref<VoronoiWorldData> m_result;
    godot::Callable m_pollCallable;
    godot::String m_errorMessage;
    std::int64_t m_status{STATUS_PENDING};
    std::int64_t m_workerTaskId{-1};
    std::uint64_t m_mainLoopId{0};

    void start(WorldGenerationSettings settings);
    void failDeferred(const godot::String &message);
    void poll();
    void finishDeferredFailure(const godot::String &message);
    void finishFailure(const godot::String &message);
    void disconnectPoll();

    static void generateOnWorker(void *userData);

    friend class VoronoiWorldGenerator;
};

} // namespace worldgen
