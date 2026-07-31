#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rect2.hpp>

#include <cstdint>

namespace worldgen {

class World;
class VoronoiWorldGenerator;

class VoronoiWorldData final : public godot::RefCounted {
    GDCLASS(VoronoiWorldData, godot::RefCounted)

public:
    enum {
        REGION_TYPE_WATER = 0,
        REGION_TYPE_LAND = 1,
    };

    VoronoiWorldData() = default;

    [[nodiscard]] std::int64_t cellCount() const noexcept;
    [[nodiscard]] godot::Rect2 bounds() const;
    [[nodiscard]] godot::PackedVector2Array sites() const;
    [[nodiscard]] godot::PackedVector2Array vertices() const;
    [[nodiscard]] godot::PackedInt32Array cellVertexOffsets() const;
    [[nodiscard]] godot::PackedInt32Array neighbors() const;
    [[nodiscard]] godot::PackedInt32Array neighborOffsets() const;
    [[nodiscard]] godot::PackedFloat64Array elevations() const;
    [[nodiscard]] godot::PackedInt32Array regionTypes() const;

protected:
    static void _bind_methods();

private:
    godot::Rect2 m_bounds;
    godot::PackedVector2Array m_sites;
    godot::PackedVector2Array m_vertices;
    godot::PackedInt32Array m_cellVertexOffsets;
    godot::PackedInt32Array m_neighbors;
    godot::PackedInt32Array m_neighborOffsets;
    godot::PackedFloat64Array m_elevations;
    godot::PackedInt32Array m_regionTypes;

    void populate(const World &world);

    friend class VoronoiWorldGenerator;
};

} // namespace worldgen
