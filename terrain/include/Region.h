#pragma once

#include "River.h"
#include "WorldDivision.h"

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace worldgen {

enum class RegionType {
    Water,
    Land,
};

class Region {
public:
    Region(CellId cell,
           double elevation,
           double seaLevel,
           std::size_t edgeCount)
        : m_cell(cell),
          m_elevation(elevation),
          m_type(elevation < seaLevel ? RegionType::Water : RegionType::Land),
          m_edgeRivers(edgeCount, INVALID_RIVER_ID) {}

    [[nodiscard]] CellId cell() const noexcept {
        return m_cell;
    }

    [[nodiscard]] double elevation() const noexcept {
        return m_elevation;
    }

    [[nodiscard]] RegionType type() const noexcept {
        return m_type;
    }

    [[nodiscard]] bool isWater() const noexcept {
        return m_type == RegionType::Water;
    }

    [[nodiscard]] bool isLand() const noexcept {
        return m_type == RegionType::Land;
    }

    [[nodiscard]] const std::vector<RiverId> &edgeRivers() const noexcept {
        return m_edgeRivers;
    }

    [[nodiscard]] RiverId riverAtEdge(std::size_t edge) const {
        return m_edgeRivers.at(edge);
    }

    [[nodiscard]] bool hasRiverAtEdge(std::size_t edge) const {
        return riverAtEdge(edge) != INVALID_RIVER_ID;
    }

    void setRiverAtEdge(std::size_t edge, RiverId river) {
        if (river == INVALID_RIVER_ID)
            throw std::invalid_argument("A region edge needs a valid river ID.");

        auto &assigned = m_edgeRivers.at(edge);
        if (assigned != INVALID_RIVER_ID && assigned != river) {
            throw std::logic_error(
                "A region edge cannot belong to two river segments.");
        }
        assigned = river;
    }

private:
    CellId m_cell;
    double m_elevation;
    RegionType m_type;
    std::vector<RiverId> m_edgeRivers;
};

} // namespace worldgen
