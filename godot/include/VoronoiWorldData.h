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
        LAND_TYPE_MOUNTAIN = 0,
        LAND_TYPE_SNOW_PEAKS = 1,
        LAND_TYPE_HILLS = 2,
        LAND_TYPE_FIELDS = 3,
        LAND_TYPE_FOREST = 4,
        LAND_TYPE_SPARSE = 5,
        LAND_TYPE_DESERT = 6,
        LAND_TYPE_SWAMP = 7,
        LAND_TYPE_RAINFOREST = 8,
        LAND_TYPE_TUNDRA = 9,
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
    [[nodiscard]] godot::PackedInt32Array landRegionIds() const;
    [[nodiscard]] godot::PackedInt32Array regionLandIndices() const;
    [[nodiscard]] godot::PackedFloat64Array landTemperatures() const;
    [[nodiscard]] godot::PackedFloat64Array landHumidities() const;
    [[nodiscard]] godot::PackedFloat64Array landVegetations() const;
    [[nodiscard]] godot::PackedInt32Array landTypes() const;
    [[nodiscard]] godot::PackedInt32Array regionTypes() const;
    [[nodiscard]] std::int64_t riverCount() const noexcept;
    [[nodiscard]] godot::PackedVector2Array riverVertices() const;
    [[nodiscard]] godot::PackedFloat64Array riverStrengths() const;
    [[nodiscard]] godot::PackedInt32Array riverOffsets() const;
    [[nodiscard]] godot::PackedInt32Array riverDownstreamIndices() const;
    [[nodiscard]] godot::PackedInt32Array cellEdgeRivers() const;
    [[nodiscard]] std::int64_t provinceCount() const noexcept;
    [[nodiscard]] godot::PackedInt32Array provinceRegionIds() const;
    [[nodiscard]] godot::PackedInt32Array provinceOffsets() const;
    [[nodiscard]] godot::PackedInt32Array provinceSeedRegionIds() const;
    [[nodiscard]] godot::PackedFloat64Array provinceRemainingScores() const;
    [[nodiscard]] godot::PackedInt32Array regionProvinceIndices() const;

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
    godot::PackedInt32Array m_landRegionIds;
    godot::PackedInt32Array m_regionLandIndices;
    godot::PackedFloat64Array m_landTemperatures;
    godot::PackedFloat64Array m_landHumidities;
    godot::PackedFloat64Array m_landVegetations;
    godot::PackedInt32Array m_landTypes;
    godot::PackedInt32Array m_regionTypes;
    godot::PackedVector2Array m_riverVertices;
    godot::PackedFloat64Array m_riverStrengths;
    godot::PackedInt32Array m_riverOffsets;
    godot::PackedInt32Array m_riverDownstreamIndices;
    godot::PackedInt32Array m_cellEdgeRivers;
    godot::PackedInt32Array m_provinceRegionIds;
    godot::PackedInt32Array m_provinceOffsets;
    godot::PackedInt32Array m_provinceSeedRegionIds;
    godot::PackedFloat64Array m_provinceRemainingScores;
    godot::PackedInt32Array m_regionProvinceIndices;

    void populate(const World &world);

    friend class VoronoiWorldGenerator;
};

} // namespace worldgen
