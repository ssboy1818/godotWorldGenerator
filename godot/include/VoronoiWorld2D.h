#pragma once

#include "VoronoiWorldData.h"
#include "VoronoiWorldGenerator.h"
#include "WorldgenSettings.h"

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace worldgen {

class VoronoiWorld2D final : public godot::Node2D {
    GDCLASS(VoronoiWorld2D, godot::Node2D)

public:
    VoronoiWorld2D();

    void setSettings(const godot::Ref<WorldgenSettings> &settings);
    [[nodiscard]] godot::Ref<WorldgenSettings> settings() const;

    [[nodiscard]] godot::Ref<VoronoiWorldData> generate();

protected:
    static void _bind_methods();

private:
    godot::Ref<VoronoiWorldGenerator> m_generator;
};

} // namespace worldgen
