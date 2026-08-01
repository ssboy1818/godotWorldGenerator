#pragma once

#include "Region.h"

#include <vector>

namespace worldgen {

class Province {
public:
    Province(RegionId seedRegion,
             std::vector<RegionId> regionIds,
             double remainingScore);

    [[nodiscard]] RegionId seedRegion() const noexcept;
    [[nodiscard]] const std::vector<RegionId> &regionIds() const noexcept;
    [[nodiscard]] double remainingScore() const noexcept;

private:
    RegionId m_seedRegion;
    std::vector<RegionId> m_regionIds;
    double m_remainingScore;
};

} // namespace worldgen
