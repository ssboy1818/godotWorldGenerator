#pragma once

#include "VoronoiWorldData.h"
#include "VoronoiWorldGenerationTask.h"
#include "WorldgenSettings.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace worldgen {

class VoronoiWorldGenerator final : public godot::RefCounted {
    GDCLASS(VoronoiWorldGenerator, godot::RefCounted)

public:
    VoronoiWorldGenerator();

    void setSettings(const godot::Ref<WorldgenSettings> &settings);
    [[nodiscard]] godot::Ref<WorldgenSettings> settings() const;

    [[nodiscard]] godot::Ref<VoronoiWorldData> generate();
    [[nodiscard]] godot::Ref<VoronoiWorldGenerationTask> generateAsync();

protected:
    static void _bind_methods();

private:
    godot::Ref<WorldgenSettings> m_settings;
};

} // namespace worldgen
