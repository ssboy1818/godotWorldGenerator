#pragma once

#include "LandType.h"
#include "River.h"
#include "WorldDivision.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace worldgen {

using RegionId = std::uint32_t;
inline constexpr auto INVALID_REGION_ID = std::numeric_limits<RegionId>::max();
using LandClimateId = std::uint32_t;
inline constexpr auto INVALID_LAND_CLIMATE_ID =
    std::numeric_limits<LandClimateId>::max();
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
           LandClimateId landClimateId)
        : m_cell(cell),
          m_elevation(elevation),
          m_seaLevel(seaLevel),
          m_type(elevation < seaLevel ? RegionType::Water : RegionType::Land),
          m_landClimateId(landClimateId),
          m_edgeRivers(edgeCount, INVALID_RIVER_ID) {
        if (isLand() != hasLandClimate()) {
            throw std::invalid_argument(
                "Only land regions can reference a land climate sample.");
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

    [[nodiscard]] double seaLevel() const noexcept {
        return m_seaLevel;
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

    [[nodiscard]] LandClimateId landClimateId() const noexcept {
        return m_landClimateId;
    }

    [[nodiscard]] bool hasLandClimate() const noexcept {
        return m_landClimateId != INVALID_LAND_CLIMATE_ID;
    }

    [[nodiscard]] LandType landType() const {
        if (!m_landType.has_value())
            throw std::logic_error("A region does not have a land type.");
        return *m_landType;
    }

    [[nodiscard]] bool hasLandType() const noexcept {
        return m_landType.has_value();
    }

    void setLandType(LandType landType) {
        if (!isLand())
            throw std::logic_error("A water region cannot have a land type.");
        if (!isValidLandType(landType))
            throw std::invalid_argument("A region needs a valid land type.");
        if (m_landType.has_value() && *m_landType != landType) {
            throw std::logic_error(
                "A land region cannot have two land types.");
        }
        m_landType = landType;
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

    void reassignProvinceId(ProvinceId currentProvince,
                            ProvinceId replacementProvince) {
        if (currentProvince == INVALID_PROVINCE_ID
            || replacementProvince == INVALID_PROVINCE_ID) {
            throw std::invalid_argument(
                "A region needs valid current and replacement province IDs.");
        }
        if (m_provinceId != currentProvince) {
            throw std::logic_error(
                "A region cannot be reassigned from a province it does not belong to.");
        }
        m_provinceId = replacementProvince;
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
    double m_seaLevel;
    RegionType m_type;
    LandClimateId m_landClimateId;
    std::optional<LandType> m_landType;
    ProvinceId m_provinceId{INVALID_PROVINCE_ID};
    std::vector<RiverId> m_edgeRivers;
};

} // namespace worldgen
