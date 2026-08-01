#include "Province.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace worldgen {

Province::Province(RegionId seedRegion,
                   std::vector<RegionId> regionIds,
                   double remainingScore)
    : m_seedRegion(seedRegion),
      m_regionIds(std::move(regionIds)),
      m_remainingScore(remainingScore) {
    if (m_seedRegion == INVALID_REGION_ID
        || m_regionIds.empty()
        || m_regionIds.front() != m_seedRegion) {
        throw std::invalid_argument(
            "A province needs a valid seed as its first region.");
    }
    if (!std::isfinite(m_remainingScore) || m_remainingScore < 0.0) {
        throw std::invalid_argument(
            "A province needs a finite non-negative remaining score.");
    }
}

RegionId Province::seedRegion() const noexcept {
    return m_seedRegion;
}

const std::vector<RegionId> &Province::regionIds() const noexcept {
    return m_regionIds;
}

double Province::remainingScore() const noexcept {
    return m_remainingScore;
}

} // namespace worldgen
