#include "VoronoiWorldData.h"

#include "Region.h"
#include "World.h"

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace worldgen {

namespace {

[[nodiscard]] godot::Vector2 toGodotVector(Vector2d value) {
    return {
        static_cast<godot::real_t>(value.x),
        static_cast<godot::real_t>(value.y),
    };
}

void addPackedCount(std::size_t &total,
                    std::size_t additional,
                    const char *description) {
    constexpr auto maximum = static_cast<std::size_t>(
        std::numeric_limits<std::int32_t>::max());
    if (additional > maximum - total)
        throw std::length_error(description);
    total += additional;
}

template <class PackedArray>
void resizePacked(PackedArray &array,
                  std::size_t size,
                  const char *description) {
    if (array.resize(static_cast<std::int64_t>(size)) != godot::OK)
        throw std::runtime_error(description);
}

} // namespace

void VoronoiWorldData::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("get_cell_count"),
                                &VoronoiWorldData::cellCount);
    godot::ClassDB::bind_method(godot::D_METHOD("get_bounds"),
                                &VoronoiWorldData::bounds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_sites"),
                                &VoronoiWorldData::sites);
    godot::ClassDB::bind_method(godot::D_METHOD("get_vertices"),
                                &VoronoiWorldData::vertices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_cell_vertex_offsets"),
                                &VoronoiWorldData::cellVertexOffsets);
    godot::ClassDB::bind_method(godot::D_METHOD("get_neighbors"),
                                &VoronoiWorldData::neighbors);
    godot::ClassDB::bind_method(godot::D_METHOD("get_neighbor_offsets"),
                                &VoronoiWorldData::neighborOffsets);
    godot::ClassDB::bind_method(godot::D_METHOD("get_elevations"),
                                &VoronoiWorldData::elevations);
    godot::ClassDB::bind_method(godot::D_METHOD("get_land_region_ids"),
                                &VoronoiWorldData::landRegionIds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_region_land_indices"),
                                &VoronoiWorldData::regionLandIndices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_land_temperatures"),
                                &VoronoiWorldData::landTemperatures);
    godot::ClassDB::bind_method(godot::D_METHOD("get_land_humidities"),
                                &VoronoiWorldData::landHumidities);
    godot::ClassDB::bind_method(godot::D_METHOD("get_land_vegetations"),
                                &VoronoiWorldData::landVegetations);
    godot::ClassDB::bind_method(godot::D_METHOD("get_land_types"),
                                &VoronoiWorldData::landTypes);
    godot::ClassDB::bind_method(godot::D_METHOD("get_region_types"),
                                &VoronoiWorldData::regionTypes);
    godot::ClassDB::bind_method(godot::D_METHOD("get_river_count"),
                                &VoronoiWorldData::riverCount);
    godot::ClassDB::bind_method(godot::D_METHOD("get_river_vertices"),
                                &VoronoiWorldData::riverVertices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_river_strengths"),
                                &VoronoiWorldData::riverStrengths);
    godot::ClassDB::bind_method(godot::D_METHOD("get_river_offsets"),
                                &VoronoiWorldData::riverOffsets);
    godot::ClassDB::bind_method(godot::D_METHOD("get_river_downstream_indices"),
                                &VoronoiWorldData::riverDownstreamIndices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_cell_edge_rivers"),
                                &VoronoiWorldData::cellEdgeRivers);
    godot::ClassDB::bind_method(godot::D_METHOD("get_province_count"),
                                &VoronoiWorldData::provinceCount);
    godot::ClassDB::bind_method(godot::D_METHOD("get_province_region_ids"),
                                &VoronoiWorldData::provinceRegionIds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_province_offsets"),
                                &VoronoiWorldData::provinceOffsets);
    godot::ClassDB::bind_method(godot::D_METHOD("get_province_seed_region_ids"),
                                &VoronoiWorldData::provinceSeedRegionIds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_province_remaining_scores"),
                                &VoronoiWorldData::provinceRemainingScores);
    godot::ClassDB::bind_method(godot::D_METHOD("get_region_province_indices"),
                                &VoronoiWorldData::regionProvinceIndices);

    BIND_CONSTANT(REGION_TYPE_WATER);
    BIND_CONSTANT(REGION_TYPE_LAND);
    BIND_CONSTANT(LAND_TYPE_MOUNTAIN);
    BIND_CONSTANT(LAND_TYPE_SNOW_PEAKS);
    BIND_CONSTANT(LAND_TYPE_HILLS);
    BIND_CONSTANT(LAND_TYPE_FIELDS);
    BIND_CONSTANT(LAND_TYPE_FOREST);
    BIND_CONSTANT(LAND_TYPE_SPARSE);
    BIND_CONSTANT(LAND_TYPE_DESERT);
    BIND_CONSTANT(LAND_TYPE_BEACH);
    BIND_CONSTANT(LAND_TYPE_SWAMP);
    BIND_CONSTANT(LAND_TYPE_RAINFOREST);
    BIND_CONSTANT(LAND_TYPE_TUNDRA);

    constexpr auto readOnly = godot::PROPERTY_USAGE_DEFAULT
                              | godot::PROPERTY_USAGE_READ_ONLY;
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT,
                                     "cell_count",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_cell_count");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::RECT2,
                                     "bounds",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_bounds");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_VECTOR2_ARRAY,
                                     "sites",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_sites");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_VECTOR2_ARRAY,
                                     "vertices",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_vertices");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "cell_vertex_offsets",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_cell_vertex_offsets");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "neighbors",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_neighbors");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "neighbor_offsets",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_neighbor_offsets");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_FLOAT64_ARRAY,
                                     "elevations",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_elevations");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "land_region_ids",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_land_region_ids");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "region_land_indices",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_region_land_indices");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_FLOAT64_ARRAY,
                                     "land_temperatures",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_land_temperatures");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_FLOAT64_ARRAY,
                                     "land_humidities",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_land_humidities");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_FLOAT64_ARRAY,
                                     "land_vegetations",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_land_vegetations");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "land_types",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_land_types");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "region_types",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_region_types");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT,
                                     "river_count",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_river_count");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_VECTOR2_ARRAY,
                                     "river_vertices",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_river_vertices");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_FLOAT64_ARRAY,
                                     "river_strengths",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_river_strengths");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "river_offsets",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_river_offsets");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "river_downstream_indices",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_river_downstream_indices");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "cell_edge_rivers",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_cell_edge_rivers");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT,
                                     "province_count",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_province_count");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "province_region_ids",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_province_region_ids");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "province_offsets",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_province_offsets");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "province_seed_region_ids",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_province_seed_region_ids");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_FLOAT64_ARRAY,
                                     "province_remaining_scores",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_province_remaining_scores");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_INT32_ARRAY,
                                     "region_province_indices",
                                     godot::PROPERTY_HINT_NONE,
                                     "",
                                     readOnly),
                 "", "get_region_province_indices");
}

std::int64_t VoronoiWorldData::cellCount() const noexcept {
    return m_sites.size();
}

godot::Rect2 VoronoiWorldData::bounds() const {
    return m_bounds;
}

godot::PackedVector2Array VoronoiWorldData::sites() const {
    return m_sites;
}

godot::PackedVector2Array VoronoiWorldData::vertices() const {
    return m_vertices;
}

godot::PackedInt32Array VoronoiWorldData::cellVertexOffsets() const {
    return m_cellVertexOffsets;
}

godot::PackedInt32Array VoronoiWorldData::neighbors() const {
    return m_neighbors;
}

godot::PackedInt32Array VoronoiWorldData::neighborOffsets() const {
    return m_neighborOffsets;
}

godot::PackedFloat64Array VoronoiWorldData::elevations() const {
    return m_elevations;
}

godot::PackedInt32Array VoronoiWorldData::landRegionIds() const {
    return m_landRegionIds;
}

godot::PackedInt32Array VoronoiWorldData::regionLandIndices() const {
    return m_regionLandIndices;
}

godot::PackedFloat64Array VoronoiWorldData::landTemperatures() const {
    return m_landTemperatures;
}

godot::PackedFloat64Array VoronoiWorldData::landHumidities() const {
    return m_landHumidities;
}

godot::PackedFloat64Array VoronoiWorldData::landVegetations() const {
    return m_landVegetations;
}

godot::PackedInt32Array VoronoiWorldData::landTypes() const {
    return m_landTypes;
}

godot::PackedInt32Array VoronoiWorldData::regionTypes() const {
    return m_regionTypes;
}

std::int64_t VoronoiWorldData::riverCount() const noexcept {
    return m_riverOffsets.size() == 0 ? 0 : m_riverOffsets.size() - 1;
}

godot::PackedVector2Array VoronoiWorldData::riverVertices() const {
    return m_riverVertices;
}

godot::PackedFloat64Array VoronoiWorldData::riverStrengths() const {
    return m_riverStrengths;
}

godot::PackedInt32Array VoronoiWorldData::riverOffsets() const {
    return m_riverOffsets;
}

godot::PackedInt32Array VoronoiWorldData::riverDownstreamIndices() const {
    return m_riverDownstreamIndices;
}

godot::PackedInt32Array VoronoiWorldData::cellEdgeRivers() const {
    return m_cellEdgeRivers;
}

std::int64_t VoronoiWorldData::provinceCount() const noexcept {
    return m_provinceOffsets.size() == 0 ? 0 : m_provinceOffsets.size() - 1;
}

godot::PackedInt32Array VoronoiWorldData::provinceRegionIds() const {
    return m_provinceRegionIds;
}

godot::PackedInt32Array VoronoiWorldData::provinceOffsets() const {
    return m_provinceOffsets;
}

godot::PackedInt32Array VoronoiWorldData::provinceSeedRegionIds() const {
    return m_provinceSeedRegionIds;
}

godot::PackedFloat64Array VoronoiWorldData::provinceRemainingScores() const {
    return m_provinceRemainingScores;
}

godot::PackedInt32Array VoronoiWorldData::regionProvinceIndices() const {
    return m_regionProvinceIndices;
}

void VoronoiWorldData::populate(const World &world) {
    const auto &boundingBox = world.boundingBox();
    m_bounds = {
        toGodotVector(boundingBox.min),
        toGodotVector(boundingBox.max - boundingBox.min),
    };

    const auto &cells = world.division().cells;
    constexpr auto maximum = static_cast<std::size_t>(
        std::numeric_limits<std::int32_t>::max());
    if (cells.size() >= maximum)
        throw std::length_error("The generated world has too many cells for packed offsets.");

    std::size_t vertexCount = 0;
    std::size_t neighborCount = 0;
    for (const auto &cell : cells) {
        addPackedCount(vertexCount,
                       cell.vertices.size(),
                       "The generated world has too many vertices for PackedInt32Array offsets.");
        addPackedCount(neighborCount,
                       cell.neighbors.size(),
                       "The generated world has too many neighbors for PackedInt32Array offsets.");
    }

    resizePacked(m_sites, cells.size(), "Unable to allocate the packed site array.");
    resizePacked(m_vertices, vertexCount, "Unable to allocate the packed vertex array.");
    resizePacked(m_cellVertexOffsets,
                 cells.size() + 1,
                 "Unable to allocate the cell vertex offsets.");
    resizePacked(m_neighbors, neighborCount, "Unable to allocate the packed neighbor array.");
    resizePacked(m_neighborOffsets,
                 cells.size() + 1,
                 "Unable to allocate the neighbor offsets.");
    resizePacked(m_elevations, cells.size(), "Unable to allocate the elevation array.");
    resizePacked(m_regionTypes, cells.size(), "Unable to allocate the region type array.");
    resizePacked(m_cellEdgeRivers,
                 vertexCount,
                 "Unable to allocate the cell edge river array.");

    auto *sites = m_sites.ptrw();
    auto *vertices = m_vertices.ptrw();
    auto *cellVertexOffsets = m_cellVertexOffsets.ptrw();
    auto *neighbors = m_neighbors.ptrw();
    auto *neighborOffsets = m_neighborOffsets.ptrw();

    std::size_t vertexOffset = 0;
    std::size_t neighborOffset = 0;
    for (std::size_t cellIndex = 0; cellIndex < cells.size(); ++cellIndex) {
        const auto &cell = cells[cellIndex];
        if (cell.id != cellIndex)
            throw std::logic_error("Generated cell IDs are not contiguous.");

        sites[cellIndex] = toGodotVector(cell.sitePosition);
        cellVertexOffsets[cellIndex] = static_cast<std::int32_t>(vertexOffset);
        for (const auto &vertex : cell.vertices)
            vertices[vertexOffset++] = toGodotVector(vertex);

        neighborOffsets[cellIndex] = static_cast<std::int32_t>(neighborOffset);
        for (const auto neighbor : cell.neighbors) {
            if (neighbor >= cells.size())
                throw std::logic_error("A generated cell references an invalid neighbor.");
            neighbors[neighborOffset++] = static_cast<std::int32_t>(neighbor);
        }
    }
    cellVertexOffsets[cells.size()] = static_cast<std::int32_t>(vertexOffset);
    neighborOffsets[cells.size()] = static_cast<std::int32_t>(neighborOffset);

    if (world.regions().size() != cells.size())
        throw std::logic_error("The generated world does not have one region per cell.");
    const auto &landClimates = world.landClimates();
    if (landClimates.size() > cells.size()) {
        throw std::logic_error(
            "The generated world has more land climates than regions.");
    }
    resizePacked(m_landRegionIds,
                 landClimates.size(),
                 "Unable to allocate the land region ID array.");
    resizePacked(m_regionLandIndices,
                 cells.size(),
                 "Unable to allocate the region land index array.");
    resizePacked(m_landTemperatures,
                 landClimates.size(),
                 "Unable to allocate the land temperature array.");
    resizePacked(m_landHumidities,
                 landClimates.size(),
                 "Unable to allocate the land humidity array.");
    resizePacked(m_landVegetations,
                 landClimates.size(),
                 "Unable to allocate the land vegetation array.");
    resizePacked(m_landTypes,
                 landClimates.size(),
                 "Unable to allocate the land type array.");
    const auto &rivers = world.rivers();
    if (rivers.size() >= maximum)
        throw std::length_error("The generated world has too many rivers for packed offsets.");

    auto *elevations = m_elevations.ptrw();
    auto *landRegionIds = m_landRegionIds.ptrw();
    auto *regionLandIndices = m_regionLandIndices.ptrw();
    auto *landTemperatures = m_landTemperatures.ptrw();
    auto *landHumidities = m_landHumidities.ptrw();
    auto *landVegetations = m_landVegetations.ptrw();
    auto *landTypes = m_landTypes.ptrw();
    auto *regionTypes = m_regionTypes.ptrw();
    auto *cellEdgeRivers = m_cellEdgeRivers.ptrw();
    std::vector<bool> populated(cells.size(), false);
    std::vector<bool> populatedLandClimates(landClimates.size(), false);
    for (std::size_t region = 0; region < cells.size(); ++region)
        regionLandIndices[region] = -1;
    for (const auto &region : world.regions()) {
        const auto cell = static_cast<std::size_t>(region.cell());
        if (cell >= cells.size() || populated[cell])
            throw std::logic_error("The generated world has invalid region cell IDs.");

        elevations[cell] = region.elevation();
        regionTypes[cell] = region.isWater() ? REGION_TYPE_WATER : REGION_TYPE_LAND;
        if (region.isWater()) {
            if (region.hasLandClimate() || region.hasLandType()) {
                throw std::logic_error(
                    "A generated water region has land-only data.");
            }
        } else {
            const auto landClimate = static_cast<std::size_t>(
                region.landClimateId());
            if (landClimate >= landClimates.size()
                || populatedLandClimates[landClimate]) {
                throw std::logic_error(
                    "A generated land region references an invalid land climate.");
            }
            const auto &climate = landClimates[landClimate];
            if (!std::isfinite(climate.temperature)
                || climate.temperature < -50.0 || climate.temperature > 50.0
                || !std::isfinite(climate.humidity)
                || climate.humidity < 0.0 || climate.humidity > 1.0
                || !std::isfinite(climate.vegetation)
                || climate.vegetation < 0.0 || climate.vegetation > 1.0) {
                throw std::logic_error(
                    "A generated land climate contains invalid values.");
            }
            landRegionIds[landClimate] = static_cast<std::int32_t>(cell);
            regionLandIndices[cell] = static_cast<std::int32_t>(landClimate);
            landTemperatures[landClimate] = climate.temperature;
            landHumidities[landClimate] = climate.humidity;
            landVegetations[landClimate] = climate.vegetation;
            if (!region.hasLandType()) {
                throw std::logic_error(
                    "A generated land region does not have a land type.");
            }
            landTypes[landClimate] = static_cast<std::int32_t>(
                region.landType());
            populatedLandClimates[landClimate] = true;
        }
        if (region.edgeRivers().size() != cells[cell].vertices.size()) {
            throw std::logic_error(
                "A generated region does not have one river ID per polygon edge.");
        }
        const auto edgeOffset = static_cast<std::size_t>(cellVertexOffsets[cell]);
        for (std::size_t edge = 0; edge < region.edgeRivers().size(); ++edge) {
            const auto river = region.edgeRivers()[edge];
            if (river != INVALID_RIVER_ID && river >= world.rivers().size()) {
                throw std::logic_error(
                    "A generated region edge references an invalid river.");
            }
            cellEdgeRivers[edgeOffset + edge] = river == INVALID_RIVER_ID
                                                   ? -1
                                                   : static_cast<std::int32_t>(river);
        }
        populated[cell] = true;
    }
    for (const auto climatePopulated : populatedLandClimates) {
        if (!climatePopulated) {
            throw std::logic_error(
                "A generated land climate is not referenced by a region.");
        }
    }

    std::size_t riverNodeCount = 0;
    for (const auto &river : rivers) {
        if (river.nodes.size() < 2)
            throw std::logic_error("A generated river must have at least two nodes.");
        addPackedCount(riverNodeCount,
                       river.nodes.size(),
                       "The generated world has too many river nodes for packed offsets.");
    }

    resizePacked(m_riverVertices,
                 riverNodeCount,
                 "Unable to allocate the packed river vertex array.");
    resizePacked(m_riverStrengths,
                 riverNodeCount,
                 "Unable to allocate the packed river strength array.");
    resizePacked(m_riverOffsets,
                 rivers.size() + 1,
                 "Unable to allocate the river offsets.");
    resizePacked(m_riverDownstreamIndices,
                 rivers.size(),
                 "Unable to allocate the downstream river index array.");

    auto *riverVertices = m_riverVertices.ptrw();
    auto *riverStrengths = m_riverStrengths.ptrw();
    auto *riverOffsets = m_riverOffsets.ptrw();
    auto *riverDownstreamIndices = m_riverDownstreamIndices.ptrw();
    std::size_t riverNodeOffset = 0;
    for (std::size_t riverIndex = 0; riverIndex < rivers.size(); ++riverIndex) {
        const auto &river = rivers[riverIndex];
        riverOffsets[riverIndex] = static_cast<std::int32_t>(riverNodeOffset);
        for (const auto &node : river.nodes) {
            if (!boundingBox.contains(node.vertex)
                || !std::isfinite(node.strength) || node.strength <= 0.0) {
                throw std::logic_error("A generated river contains an invalid node.");
            }
            riverVertices[riverNodeOffset] = toGodotVector(node.vertex);
            riverStrengths[riverNodeOffset] = node.strength;
            ++riverNodeOffset;
        }

        if (river.downstreamRiver == INVALID_RIVER_ID) {
            riverDownstreamIndices[riverIndex] = -1;
            continue;
        }
        if (river.downstreamRiver >= rivers.size()
            || river.downstreamRiver == riverIndex) {
            throw std::logic_error("A generated river has an invalid downstream river.");
        }
        const auto &downstream = rivers[river.downstreamRiver];
        if (river.nodes.back().vertex != downstream.nodes.front().vertex) {
            throw std::logic_error(
                "Linked river segments do not meet at a shared vertex.");
        }
        riverDownstreamIndices[riverIndex] = static_cast<std::int32_t>(
            river.downstreamRiver);
    }
    riverOffsets[rivers.size()] = static_cast<std::int32_t>(riverNodeOffset);

    const auto &provinces = world.provinces();
    auto landRegionCount = std::size_t{0};
    for (const auto &region : world.regions()) {
        if (region.isLand())
            ++landRegionCount;
    }
    if (provinces.size() > landRegionCount) {
        throw std::logic_error(
            "The generated world has more provinces than land regions.");
    }
    resizePacked(m_provinceRegionIds,
                 landRegionCount,
                 "Unable to allocate the province region ID array.");
    resizePacked(m_provinceOffsets,
                 provinces.size() + 1,
                 "Unable to allocate the province offsets.");
    resizePacked(m_provinceSeedRegionIds,
                 provinces.size(),
                 "Unable to allocate the province seed region ID array.");
    resizePacked(m_provinceRemainingScores,
                 provinces.size(),
                 "Unable to allocate the province remaining score array.");
    resizePacked(m_regionProvinceIndices,
                 cells.size(),
                 "Unable to allocate the region province index array.");

    auto *provinceRegionIds = m_provinceRegionIds.ptrw();
    auto *provinceOffsets = m_provinceOffsets.ptrw();
    auto *provinceSeedRegionIds = m_provinceSeedRegionIds.ptrw();
    auto *provinceRemainingScores = m_provinceRemainingScores.ptrw();
    auto *regionProvinceIndices = m_regionProvinceIndices.ptrw();
    for (std::size_t region = 0; region < cells.size(); ++region)
        regionProvinceIndices[region] = -1;
    std::vector<bool> assignedRegions(cells.size(), false);
    std::size_t provinceRegionOffset = 0;
    for (std::size_t provinceIndex = 0;
         provinceIndex < provinces.size();
         ++provinceIndex) {
        const auto &province = provinces[provinceIndex];
        if (province.regionIds().empty()
            || province.seedRegion() != province.regionIds().front()
            || !std::isfinite(province.remainingScore())
            || province.remainingScore() < 0.0) {
            throw std::logic_error("A generated province is invalid.");
        }

        provinceOffsets[provinceIndex] = static_cast<std::int32_t>(
            provinceRegionOffset);
        provinceSeedRegionIds[provinceIndex] = static_cast<std::int32_t>(
            province.seedRegion());
        provinceRemainingScores[provinceIndex] = province.remainingScore();
        for (const auto region : province.regionIds()) {
            const auto regionIndex = static_cast<std::size_t>(region);
            if (regionIndex >= cells.size() || assignedRegions[regionIndex]) {
                throw std::logic_error(
                    "Generated provinces contain invalid or duplicate region IDs.");
            }
            if (world.regions()[regionIndex].isWater()) {
                throw std::logic_error(
                    "A generated province contains a water region.");
            }
            if (world.regions()[regionIndex].provinceId() != provinceIndex) {
                throw std::logic_error(
                    "A generated region references an inconsistent province ID.");
            }
            provinceRegionIds[provinceRegionOffset++] = static_cast<std::int32_t>(
                region);
            regionProvinceIndices[regionIndex] = static_cast<std::int32_t>(
                provinceIndex);
            assignedRegions[regionIndex] = true;
        }
    }
    if (provinceRegionOffset != landRegionCount) {
        throw std::logic_error(
            "Generated provinces do not assign every land region exactly once.");
    }
    for (std::size_t region = 0; region < cells.size(); ++region) {
        const auto &generatedRegion = world.regions()[region];
        if (generatedRegion.isLand()
            != static_cast<bool>(assignedRegions[region])) {
            throw std::logic_error(
                "Generated province ownership does not match land classification.");
        }
        if (generatedRegion.isWater()
            && (generatedRegion.hasProvince()
                || regionProvinceIndices[region] != -1)) {
            throw std::logic_error(
                "A generated water region references a province.");
        }
    }
    provinceOffsets[provinces.size()] = static_cast<std::int32_t>(
        provinceRegionOffset);
}

} // namespace worldgen
