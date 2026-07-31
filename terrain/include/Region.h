#pragma once

#include "Polygon.h"

enum class RegionType {
    Water,
    Land,
};

class Region {
public:
    Region(PolygonId cell,
           double elevation,
           double seaLevel)
        : m_cell(cell),
          m_elevation(elevation),
          m_type(elevation < seaLevel ? RegionType::Water : RegionType::Land) {}

    [[nodiscard]] PolygonId cell() const noexcept {
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
    PolygonId m_cell;
    double m_elevation;
    RegionType m_type;
};
