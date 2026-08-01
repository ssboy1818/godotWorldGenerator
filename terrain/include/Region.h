#pragma once

#include "River.h"
#include "WorldDivision.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace worldgen {

using RegionId = std::uint32_t;
inline constexpr auto INVALID_REGION_ID = std::numeric_limits<RegionId>::max();
using ProvinceId = std::uint32_t;
inline constexpr auto INVALID_PROVINCE_ID = std::numeric_limits<ProvinceId>::max();

enum class RegionType {
    Water,
    Land,
};

class Region {
public:
    Region(CellId cell,
           double elevation,
           double seaLevel,
           std::size_t edgeCount,
           double temperature,
           double humidity,
           double vegetation)
        : m_cell(cell),
          m_elevation(elevation),
          m_type(elevation < seaLevel ? RegionType::Water : RegionType::Land),
          m_temperature(temperature),
          m_humidity(humidity),
          m_vegetation(vegetation),
          m_edgeRivers(edgeCount, INVALID_RIVER_ID) {
        if (!std::isfinite(m_temperature)
            || m_temperature < -50.0 || m_temperature > 50.0) {
            throw std::invalid_argument(
                "A region temperature must be between -50 and 50.");
        }
        if (!std::isfinite(m_humidity)
            || m_humidity < 0.0 || m_humidity > 1.0) {
            throw std::invalid_argument(
                "A region humidity value must be between zero and one.");
        }
        if (!std::isfinite(m_vegetation)
            || m_vegetation < 0.0 || m_vegetation > 1.0) {
            throw std::invalid_argument(
                "A region vegetation value must be between zero and one.");
        }
    }

    [[nodiscard]] CellId cell() const noexcept {
        return m_cell;
    }

    [[nodiscard]] RegionId id() const noexcept {
        return static_cast<RegionId>(m_cell);
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

    [[nodiscard]] double temperature() const noexcept {
        return m_temperature;
    }

    [[nodiscard]] double humidity() const noexcept {
        return m_humidity;
    }

    [[nodiscard]] double vegetation() const noexcept {
        return m_vegetation;
    }

    [[nodiscard]] ProvinceId provinceId() const noexcept {
        return m_provinceId;
    }

    [[nodiscard]] bool hasProvince() const noexcept {
        return m_provinceId != INVALID_PROVINCE_ID;
    }

    void setProvinceId(ProvinceId province) {
        if (province == INVALID_PROVINCE_ID)
            throw std::invalid_argument("A region needs a valid province ID.");
        if (hasProvince() && m_provinceId != province) {
            throw std::logic_error(
                "A region cannot belong to two provinces.");
        }
        m_provinceId = province;
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
    double m_temperature;
    double m_humidity;
    double m_vegetation;
    ProvinceId m_provinceId{INVALID_PROVINCE_ID};
    std::vector<RiverId> m_edgeRivers;
};

} // namespace worldgen
