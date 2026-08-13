#include "ProvinceGenerator.h"

#include "Id.h"
#include "NumericalPolicy.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace worldgen {

namespace {

struct Claim {
    double cost;
    RegionId region;
    RegionId fromRegion;
};

struct ClaimCostParameters {
    double riverContribution;
    double elevationContribution;
    double distanceContribution;
    double shortBorderContribution;
    double landTypeContribution;
    double baseCost;
    double sharedEdgeTolerance;
    double averageCellLength;
};

[[nodiscard]] long double quantizedCost(double cost) noexcept {
    // EPS-sized buckets provide a transitive equivalence relation, unlike a
    // pairwise abs(left - right) <= EPS comparison in a heap comparator.
    return std::floor(static_cast<long double>(cost)
                      / static_cast<long double>(EPS));
}

struct MoreExpensiveClaim {
    [[nodiscard]] bool operator()(const Claim &left,
                                  const Claim &right) const noexcept {
        const auto leftCost = quantizedCost(left.cost);
        const auto rightCost = quantizedCost(right.cost);
        if (leftCost != rightCost)
            return leftCost > rightCost;
        if (left.region != right.region)
            return left.region > right.region;
        return left.fromRegion > right.fromRegion;
    }
};

[[nodiscard]] double segmentSharedBoundaryLength(
    Vector2d firstStart,
    Vector2d firstEnd,
    Vector2d secondStart,
    Vector2d secondEnd,
    double tolerance) noexcept {
    const auto firstDirection = firstEnd - firstStart;
    const auto secondDirection = secondEnd - secondStart;
    const auto firstLength = firstDirection.length();
    const auto secondLength = secondDirection.length();
    if (firstLength <= tolerance || secondLength <= tolerance)
        return 0.0;

    const Vector2d unit{firstDirection.x / firstLength,
                        firstDirection.y / firstLength};
    const auto perpendicularDistance = [&](Vector2d point) {
        const auto relative = point - firstStart;
        return std::abs(relative.x * unit.y - relative.y * unit.x);
    };
    if (perpendicularDistance(secondStart) > tolerance
        || perpendicularDistance(secondEnd) > tolerance) {
        return 0.0;
    }

    const auto project = [&](Vector2d point) {
        const auto relative = point - firstStart;
        return relative.x * unit.x + relative.y * unit.y;
    };
    auto secondMinimum = project(secondStart);
    auto secondMaximum = project(secondEnd);
    if (secondMinimum > secondMaximum)
        std::swap(secondMinimum, secondMaximum);

    const auto overlap = std::min(firstLength, secondMaximum)
                         - std::max(0.0, secondMinimum);
    return overlap > tolerance ? overlap : 0.0;
}

[[nodiscard]] double sharedBoundaryLength(const Cell &firstCell,
                                          const Cell &secondCell,
                                          double tolerance) noexcept {
    auto length = 0.0;
    for (std::size_t firstEdge = 0;
         firstEdge < firstCell.vertices.size();
         ++firstEdge) {
        const auto firstNext = (firstEdge + 1) % firstCell.vertices.size();
        for (std::size_t secondEdge = 0;
             secondEdge < secondCell.vertices.size();
             ++secondEdge) {
            const auto secondNext = (secondEdge + 1)
                                    % secondCell.vertices.size();
            length += segmentSharedBoundaryLength(
                firstCell.vertices[firstEdge],
                firstCell.vertices[firstNext],
                secondCell.vertices[secondEdge],
                secondCell.vertices[secondNext],
                tolerance);
        }
    }
    return length;
}

[[nodiscard]] bool riverEdgeTouchesCell(const Cell &riverCell,
                                        const Region &riverRegion,
                                        const Cell &otherCell,
                                        double tolerance) {
    if (riverRegion.edgeRivers().size() != riverCell.vertices.size()) {
        throw std::logic_error(
            "Province generation requires one river ID per region edge.");
    }

    for (std::size_t edge = 0; edge < riverCell.vertices.size(); ++edge) {
        if (!riverRegion.hasRiverAtEdge(edge))
            continue;

        const auto riverNext = (edge + 1) % riverCell.vertices.size();
        for (std::size_t otherEdge = 0;
             otherEdge < otherCell.vertices.size();
             ++otherEdge) {
            const auto otherNext = (otherEdge + 1) % otherCell.vertices.size();
            if (segmentSharedBoundaryLength(riverCell.vertices[edge],
                                            riverCell.vertices[riverNext],
                                            otherCell.vertices[otherEdge],
                                            otherCell.vertices[otherNext],
                                            tolerance)
                > 0.0) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool sharedBoundaryHasRiver(
    const Cell &firstCell,
    const Region &firstRegion,
    const Cell &secondCell,
    const Region &secondRegion,
    double tolerance) {
    return riverEdgeTouchesCell(firstCell,
                                firstRegion,
                                secondCell,
                                tolerance)
        || riverEdgeTouchesCell(secondCell,
                                secondRegion,
                                firstCell,
                                tolerance);
}

[[nodiscard]] double normalizedDistance(const BoundingBox &boundingBox,
                                        Vector2d first,
                                        Vector2d second) noexcept {
    const auto deltaX = static_cast<long double>(first.x)
                        - static_cast<long double>(second.x);
    const auto deltaY = static_cast<long double>(first.y)
                        - static_cast<long double>(second.y);
    const auto width = static_cast<long double>(boundingBox.max.x)
                       - static_cast<long double>(boundingBox.min.x);
    const auto height = static_cast<long double>(boundingBox.max.y)
                        - static_cast<long double>(boundingBox.min.y);
    return static_cast<double>(std::hypot(deltaX, deltaY)
                               / std::hypot(width, height));
}

[[nodiscard]] double provinceClaimCost(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    const std::vector<Region *> &regions,
    const ClaimCostParameters &parameters,
    RegionId provinceSeed,
    RegionId fromRegion,
    RegionId claimedRegion) {
    const auto seedIndex = static_cast<std::size_t>(provinceSeed);
    const auto fromIndex = static_cast<std::size_t>(fromRegion);
    const auto claimedIndex = static_cast<std::size_t>(claimedRegion);
    if (seedIndex >= regions.size()
        || fromIndex >= regions.size()
        || claimedIndex >= regions.size()) {
        throw std::logic_error(
            "Province generation received an invalid claim region.");
    }

    const auto &seed = *regions[seedIndex];
    const auto &from = *regions[fromIndex];
    const auto &claimed = *regions[claimedIndex];
    const auto &fromCell = division.cells[fromIndex];
    const auto &claimedCell = division.cells[claimedIndex];
    const auto crossesRiver = sharedBoundaryHasRiver(
        fromCell,
        from,
        claimedCell,
        claimed,
        parameters.sharedEdgeTolerance);
    const auto elevationDifference = std::abs(
        from.elevation() - claimed.elevation());
    const auto distance = normalizedDistance(
        boundingBox,
        division.cells[seedIndex].sitePosition,
        claimedCell.sitePosition);

    auto shortBorderPenalty = 0.0;
    if (parameters.shortBorderContribution > 0.0) {
        const auto borderLength = sharedBoundaryLength(
            fromCell,
            claimedCell,
            parameters.sharedEdgeTolerance);
        if (!std::isfinite(borderLength)
            || borderLength <= parameters.sharedEdgeTolerance) {
            throw std::logic_error(
                "Neighboring regions do not share a measurable border.");
        }
        shortBorderPenalty = parameters.shortBorderContribution
                             * std::clamp(
                                 1.0
                                     - borderLength
                                           / parameters.averageCellLength,
                                 0.0,
                                 1.0);
    }

    auto landTypePenalty = 0.0;
    if (parameters.landTypeContribution > 0.0) {
        if (!seed.hasLandType() || !claimed.hasLandType()) {
            throw std::logic_error(
                "Province land-type costs require classified land regions.");
        }
        if (seed.landType() != claimed.landType())
            landTypePenalty = parameters.landTypeContribution;
    }

    const auto cost = parameters.baseCost
                      + parameters.elevationContribution
                            * elevationDifference
                      + parameters.distanceContribution * distance
                      + (crossesRiver
                             ? parameters.riverContribution
                             : 0.0)
                      + shortBorderPenalty
                      + landTypePenalty;
    if (!std::isfinite(cost) || cost < 0.0) {
        throw std::logic_error(
            "Province generation produced an invalid claim cost.");
    }
    return cost;
}

[[nodiscard]] std::vector<Region *> indexRegions(
    const WorldDivision &division,
    std::span<Region> regions) {
    if (regions.size() != division.cells.size()) {
        throw std::logic_error(
            "Province generation requires one region per cell.");
    }

    std::vector<Region *> indexed(regions.size(), nullptr);
    for (auto &region : regions) {
        const auto id = static_cast<std::size_t>(region.id());
        if (id >= indexed.size() || indexed[id] != nullptr
            || region.hasProvince()) {
            throw std::logic_error(
                "Province generation received invalid or already assigned regions.");
        }
        indexed[id] = &region;
    }

    for (std::size_t id = 0; id < division.cells.size(); ++id) {
        if (division.cells[id].id != id || indexed[id] == nullptr) {
            throw std::logic_error(
                "Province generation requires contiguous cell and region IDs.");
        }
    }
    return indexed;
}

[[nodiscard]] std::vector<Province> mergeSmallProvinces(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    const std::vector<Region *> &regions,
    std::vector<Province> provinces,
    std::size_t minimumRegionCount,
    std::size_t maximumRegionCount,
    const ClaimCostParameters &costParameters) {
    if (minimumRegionCount <= 1 || provinces.size() <= 1)
        return provinces;

    std::vector<std::vector<RegionId>> regionNeighbors(regions.size());
    for (std::size_t region = 0; region < division.cells.size(); ++region) {
        for (const auto neighbor : division.cells[region].neighbors) {
            const auto neighborIndex = static_cast<std::size_t>(neighbor);
            if (neighborIndex >= regions.size()) {
                throw std::logic_error(
                    "Province merging received an invalid cell neighbor.");
            }
            if (neighborIndex == region)
                continue;
            regionNeighbors[region].push_back(neighbor);
            regionNeighbors[neighborIndex].push_back(
                static_cast<RegionId>(region));
        }
    }
    for (auto &neighbors : regionNeighbors) {
        std::ranges::sort(neighbors);
        const auto afterUnique = std::ranges::unique(neighbors).begin();
        neighbors.erase(afterUnique, neighbors.end());
    }

    std::vector<ProvinceId> originalOwners(regions.size(),
                                           INVALID_PROVINCE_ID);
    for (std::size_t provinceIndex = 0;
         provinceIndex < provinces.size();
         ++provinceIndex) {
        if (provinceIndex >= INVALID_PROVINCE_ID) {
            throw std::logic_error(
                "Province generation exceeded the province ID range.");
        }
        const auto provinceId = static_cast<ProvinceId>(provinceIndex);
        for (const auto regionId : provinces[provinceIndex].regionIds()) {
            const auto regionIndex = static_cast<std::size_t>(regionId);
            if (regionIndex >= regions.size()
                || regions[regionIndex]->isWater()
                || originalOwners[regionIndex] != INVALID_PROVINCE_ID
                || regions[regionIndex]->provinceId() != provinceId) {
                throw std::logic_error(
                    "Province merging received inconsistent province membership.");
            }
            originalOwners[regionIndex] = provinceId;
        }
    }
    for (std::size_t region = 0; region < regions.size(); ++region) {
        if (regions[region]->isLand()
            != (originalOwners[region] != INVALID_PROVINCE_ID)) {
            throw std::logic_error(
                "Province merging requires exactly one owner per land region.");
        }
    }

    std::vector<std::vector<ProvinceId>> provinceNeighbors(provinces.size());
    for (std::size_t region = 0; region < regions.size(); ++region) {
        const auto owner = originalOwners[region];
        if (owner == INVALID_PROVINCE_ID)
            continue;
        for (const auto neighbor : regionNeighbors[region]) {
            const auto neighborOwner = originalOwners[neighbor];
            if (neighborOwner == INVALID_PROVINCE_ID || neighborOwner == owner)
                continue;
            provinceNeighbors[owner].push_back(neighborOwner);
        }
    }
    for (auto &neighbors : provinceNeighbors) {
        std::ranges::sort(neighbors);
        const auto afterUnique = std::ranges::unique(neighbors).begin();
        neighbors.erase(afterUnique, neighbors.end());
    }

    std::vector<bool> small(provinces.size(), false);
    std::vector<bool> removed(provinces.size(), false);
    for (std::size_t province = 0; province < provinces.size(); ++province) {
        small[province] = provinces[province].regionIds().size()
                          < minimumRegionCount;
        removed[province] = small[province];
        if (maximumRegionCount != 0
            && provinces[province].regionIds().size()
                   > maximumRegionCount) {
            throw std::logic_error(
                "Province growth exceeded the maximum region count.");
        }
    }

    // Every connected group of small provinces needs a surviving destination.
    // A group touching a large province can be absorbed entirely. Otherwise its
    // largest member (then lowest ID) remains as the deterministic anchor.
    std::vector<bool> visited(provinces.size(), false);
    for (std::size_t first = 0; first < provinces.size(); ++first) {
        if (!small[first] || visited[first])
            continue;

        std::vector<ProvinceId> component;
        std::vector<ProvinceId> pending{static_cast<ProvinceId>(first)};
        visited[first] = true;
        auto touchesLargeProvince = false;
        while (!pending.empty()) {
            const auto province = pending.back();
            pending.pop_back();
            component.push_back(province);
            for (const auto neighbor : provinceNeighbors[province]) {
                if (!small[neighbor]) {
                    touchesLargeProvince = true;
                    continue;
                }
                if (!visited[neighbor]) {
                    visited[neighbor] = true;
                    pending.push_back(neighbor);
                }
            }
        }

        if (touchesLargeProvince)
            continue;

        const auto anchor = *std::ranges::min_element(
            component,
            [&](ProvinceId left, ProvinceId right) {
                const auto leftSize = provinces[left].regionIds().size();
                const auto rightSize = provinces[right].regionIds().size();
                if (leftSize != rightSize)
                    return leftSize > rightSize;
                return left < right;
            });
        removed[anchor] = false;
    }

    if (std::ranges::none_of(removed, [](bool value) { return value; }))
        return provinces;

    std::vector<ProvinceId> finalOwners(regions.size(), INVALID_PROVINCE_ID);
    auto unassignedCount = std::size_t{0};
    for (std::size_t region = 0; region < regions.size(); ++region) {
        const auto owner = originalOwners[region];
        if (owner == INVALID_PROVINCE_ID)
            continue;
        if (removed[owner]) {
            ++unassignedCount;
        } else {
            finalOwners[region] = owner;
        }
    }

    std::vector<std::vector<RegionId>> absorbedRegions(provinces.size());
    std::vector<std::size_t> finalProvinceSizes(provinces.size(), 0);
    for (std::size_t province = 0; province < provinces.size(); ++province) {
        if (!removed[province])
            finalProvinceSizes[province] = provinces[province].regionIds().size();
    }
    const auto needsReassignment = [&](RegionId region) {
        return originalOwners[region] != INVALID_PROVINCE_ID
               && finalOwners[region] == INVALID_PROVINCE_ID;
    };
    std::vector<RegionId> frontier;
    for (std::size_t region = 0; region < regions.size(); ++region) {
        if (!needsReassignment(static_cast<RegionId>(region)))
            continue;
        if (std::ranges::any_of(regionNeighbors[region],
                                [&](RegionId neighbor) {
                                    return finalOwners[neighbor]
                                           != INVALID_PROVINCE_ID;
                                })) {
            frontier.push_back(static_cast<RegionId>(region));
        }
    }

    while (unassignedCount > 0) {
        std::ranges::sort(frontier);
        const auto afterUnique = std::ranges::unique(frontier).begin();
        frontier.erase(afterUnique, frontier.end());
        if (frontier.empty()) {
            throw std::logic_error(
                "A small province could not reach a neighboring province.");
        }

        std::vector<std::pair<RegionId, ProvinceId>> assignments;
        assignments.reserve(frontier.size());
        for (const auto region : frontier) {
            if (!needsReassignment(region))
                continue;

            auto selected = INVALID_PROVINCE_ID;
            auto selectedSource = INVALID_REGION_ID;
            auto selectedCostOrder =
                std::numeric_limits<long double>::infinity();
            auto selectedHasCapacity = false;
            for (const auto neighbor : regionNeighbors[region]) {
                const auto candidate = finalOwners[neighbor];
                if (candidate == INVALID_PROVINCE_ID)
                    continue;

                const auto cost = provinceClaimCost(
                    boundingBox,
                    division,
                    regions,
                    costParameters,
                    provinces[candidate].seedRegion(),
                    neighbor,
                    region);
                const auto costOrder = quantizedCost(cost);
                const auto hasCapacity = maximumRegionCount == 0
                                         || finalProvinceSizes[candidate]
                                                < maximumRegionCount;
                const auto cheaper = costOrder < selectedCostOrder
                                     || (costOrder == selectedCostOrder
                                         && (candidate < selected
                                             || (candidate == selected
                                                 && neighbor
                                                        < selectedSource)));
                if (selected == INVALID_PROVINCE_ID
                    || (hasCapacity && !selectedHasCapacity)
                    || (hasCapacity == selectedHasCapacity && cheaper)) {
                    selected = candidate;
                    selectedSource = neighbor;
                    selectedCostOrder = costOrder;
                    selectedHasCapacity = hasCapacity;
                }
            }
            if (selected == INVALID_PROVINCE_ID)
                continue;
            assignments.emplace_back(region, selected);
            ++finalProvinceSizes[selected];
        }

        if (assignments.empty()) {
            throw std::logic_error(
                "Small-province reassignment frontier made no progress.");
        }
        if (assignments.size() > unassignedCount) {
            throw std::logic_error(
                "Small-province reassignment exceeded its unassigned land count.");
        }

        frontier.clear();
        for (const auto &[region, province] : assignments) {
            finalOwners[region] = province;
            absorbedRegions[province].push_back(region);
        }
        for (const auto &assignment : assignments) {
            const auto region = assignment.first;
            for (const auto neighbor : regionNeighbors[region]) {
                if (needsReassignment(neighbor))
                    frontier.push_back(neighbor);
            }
        }
        unassignedCount -= assignments.size();
    }

    std::vector<ProvinceId> compactedIds(provinces.size(),
                                         INVALID_PROVINCE_ID);
    std::vector<Province> merged;
    merged.reserve(provinces.size());
    for (std::size_t oldId = 0; oldId < provinces.size(); ++oldId) {
        if (removed[oldId])
            continue;
        if (merged.size() >= INVALID_PROVINCE_ID) {
            throw std::logic_error(
                "Province generation exceeded the province ID range.");
        }
        compactedIds[oldId] = static_cast<ProvinceId>(merged.size());
        auto regionIds = provinces[oldId].regionIds();
        regionIds.insert(regionIds.end(),
                         absorbedRegions[oldId].begin(),
                         absorbedRegions[oldId].end());
        merged.emplace_back(provinces[oldId].seedRegion(),
                            std::move(regionIds),
                            provinces[oldId].remainingScore());
    }

    for (std::size_t region = 0; region < regions.size(); ++region) {
        const auto oldOwner = originalOwners[region];
        if (oldOwner == INVALID_PROVINCE_ID)
            continue;
        const auto replacement = compactedIds[finalOwners[region]];
        if (replacement == INVALID_PROVINCE_ID) {
            throw std::logic_error(
                "Province merging produced an invalid compacted province ID.");
        }
        regions[region]->reassignProvinceId(oldOwner, replacement);
    }

    return merged;
}

} // namespace

std::vector<Province> generateProvinces(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    std::span<Region> regions,
    double startScore,
    double riverContribution,
    double elevationContribution,
    double distanceContribution,
    double baseCost,
    std::size_t minimumRegionCount,
    double shortBorderContribution,
    double landTypeContribution,
    std::size_t maximumRegionCount) {
    if (!std::isfinite(startScore) || startScore < 0.0
        || !std::isfinite(riverContribution) || riverContribution < 0.0
        || !std::isfinite(elevationContribution)
        || elevationContribution < 0.0
        || !std::isfinite(distanceContribution)
        || distanceContribution < 0.0
        || !std::isfinite(baseCost) || baseCost < 0.0
        || !std::isfinite(shortBorderContribution)
        || shortBorderContribution < 0.0
        || !std::isfinite(landTypeContribution)
        || landTypeContribution < 0.0) {
        throw std::invalid_argument(
            "Province scores, costs, and contributions must be finite and non-negative.");
    }
    if (!std::isfinite(baseCost
                       + riverContribution
                       + elevationContribution
                       + distanceContribution
                       + shortBorderContribution
                       + landTypeContribution)) {
        throw std::invalid_argument(
            "The maximum province claim cost must be finite.");
    }
    if (minimumRegionCount == 0) {
        throw std::invalid_argument(
            "The minimum province region count must be positive.");
    }
    if (maximumRegionCount != 0
        && maximumRegionCount < minimumRegionCount) {
        throw std::invalid_argument(
            "The maximum province region count must be zero or at least the minimum.");
    }
    if (division.cells.empty())
        return {};

    const auto indexedRegions = indexRegions(division, regions);
    if (landTypeContribution > 0.0) {
        for (const auto *region : indexedRegions) {
            if (region->isLand() && !region->hasLandType()) {
                throw std::logic_error(
                    "Province land-type costs require classified land regions.");
            }
        }
    }
    const auto sharedEdgeTolerance = numericalToleranceFor(boundingBox)
                                         .sharedEdgeLength();
    auto averageCellLength = 0.0;
    if (shortBorderContribution > 0.0) {
        const auto width = static_cast<long double>(boundingBox.max.x)
                           - static_cast<long double>(boundingBox.min.x);
        const auto height = static_cast<long double>(boundingBox.max.y)
                            - static_cast<long double>(boundingBox.min.y);
        averageCellLength = static_cast<double>(std::sqrt(
            width * height
            / static_cast<long double>(division.cells.size())));
        if (!std::isfinite(averageCellLength) || averageCellLength <= 0.0) {
            throw std::logic_error(
                "Province generation could not determine an average cell length.");
        }
    }
    const ClaimCostParameters costParameters{
        .riverContribution = riverContribution,
        .elevationContribution = elevationContribution,
        .distanceContribution = distanceContribution,
        .shortBorderContribution = shortBorderContribution,
        .landTypeContribution = landTypeContribution,
        .baseCost = baseCost,
        .sharedEdgeTolerance = sharedEdgeTolerance,
        .averageCellLength = averageCellLength,
    };
    std::vector<bool> assigned(regions.size(), false);
    for (std::size_t region = 0; region < indexedRegions.size(); ++region)
        assigned[region] = indexedRegions[region]->isWater();
    std::vector<Province> provinces;
    provinces.reserve(regions.size());

    for (std::size_t seedIndex = 0;
         seedIndex < indexedRegions.size();
         ++seedIndex) {
        if (assigned[seedIndex])
            continue;

        const auto seed = static_cast<RegionId>(seedIndex);
        const auto provinceId = static_cast<ProvinceId>(provinces.size());
        auto remainingScore = startScore;
        std::vector<RegionId> provinceRegions{seed};
        assigned[seedIndex] = true;
        indexedRegions[seedIndex]->setProvinceId(provinceId);

        std::priority_queue<Claim,
                            std::vector<Claim>,
                            MoreExpensiveClaim> frontier;
        const auto addFrontier = [&](RegionId fromRegion) {
            const auto fromIndex = static_cast<std::size_t>(fromRegion);
            const auto &fromCell = division.cells[fromIndex];
            for (const auto neighborCell : fromCell.neighbors) {
                const auto neighborIndex = static_cast<std::size_t>(neighborCell);
                if (neighborIndex >= indexedRegions.size()) {
                    throw std::logic_error(
                        "Province generation received an invalid cell neighbor.");
                }
                if (assigned[neighborIndex])
                    continue;

                const auto cost = provinceClaimCost(
                    boundingBox,
                    division,
                    indexedRegions,
                    costParameters,
                    seed,
                    fromRegion,
                    static_cast<RegionId>(neighborIndex));
                frontier.push({cost,
                               static_cast<RegionId>(neighborIndex),
                               fromRegion});
            }
        };

        addFrontier(seed);
        while (!frontier.empty()) {
            if (maximumRegionCount != 0
                && provinceRegions.size() >= maximumRegionCount) {
                break;
            }
            const auto claim = frontier.top();
            frontier.pop();
            const auto claimIndex = static_cast<std::size_t>(claim.region);
            if (assigned[claimIndex])
                continue;
            if (claim.cost > remainingScore + EPS)
                break;

            remainingScore = std::max(0.0, remainingScore - claim.cost);
            assigned[claimIndex] = true;
            indexedRegions[claimIndex]->setProvinceId(provinceId);
            provinceRegions.push_back(claim.region);
            addFrontier(claim.region);
        }

        provinces.emplace_back(seed,
                               std::move(provinceRegions),
                               remainingScore);
    }

    provinces = mergeSmallProvinces(boundingBox,
                                    division,
                                    indexedRegions,
                                    std::move(provinces),
                                    minimumRegionCount,
                                    maximumRegionCount,
                                    costParameters);

    for (const auto *region : indexedRegions) {
        if (region->isLand() != region->hasProvince()) {
            throw std::logic_error(
                "Province generation did not assign exactly the land regions.");
        }
    }

    return provinces;
}

} // namespace worldgen
