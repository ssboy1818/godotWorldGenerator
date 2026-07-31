#pragma once

#include "WorldDivision.h"

namespace worldgen {

enum class RegionType {
    Water,
    Land,
};

class Region {
public:
    Region(CellId cell,
           double elevation,
           double seaLevel)
        : m_cell(cell),
          m_elevation(elevation),
          m_type(elevation < seaLevel ? RegionType::Water : RegionType::Land) {}

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

private:
    CellId m_cell;
    double m_elevation;
    RegionType m_type;
};

} // namespace worldgen
