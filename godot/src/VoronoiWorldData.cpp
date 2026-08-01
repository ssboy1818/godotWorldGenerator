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

    BIND_CONSTANT(REGION_TYPE_WATER);
    BIND_CONSTANT(REGION_TYPE_LAND);

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
    const auto &rivers = world.rivers();
    if (rivers.size() >= maximum)
        throw std::length_error("The generated world has too many rivers for packed offsets.");

    auto *elevations = m_elevations.ptrw();
    auto *regionTypes = m_regionTypes.ptrw();
    auto *cellEdgeRivers = m_cellEdgeRivers.ptrw();
    std::vector<bool> populated(cells.size(), false);
    for (const auto &region : world.regions()) {
        const auto cell = static_cast<std::size_t>(region.cell());
        if (cell >= cells.size() || populated[cell])
            throw std::logic_error("The generated world has invalid region cell IDs.");

        elevations[cell] = region.elevation();
        regionTypes[cell] = region.isWater() ? REGION_TYPE_WATER : REGION_TYPE_LAND;
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
}

} // namespace worldgen
