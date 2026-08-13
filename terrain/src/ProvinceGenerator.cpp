#include "ProvinceGenerator.h"

#include "Id.h"
#include "NumericalPolicy.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace worldgen {

namespace {

struct Claim {
    double distanceFromSeed;
    double cost;
    ProvinceId province;
    RegionId provinceSeed;
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
        const auto leftDistance = quantizedCost(left.distanceFromSeed);
        const auto rightDistance = quantizedCost(right.distanceFromSeed);
        if (leftDistance != rightDistance)
            return leftDistance > rightDistance;
        const auto leftCost = quantizedCost(left.cost);
        const auto rightCost = quantizedCost(right.cost);
        if (leftCost != rightCost)
            return leftCost > rightCost;
        if (left.region != right.region)
            return left.region > right.region;
        if (left.provinceSeed != right.provinceSeed)
            return left.provinceSeed > right.provinceSeed;
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

struct WeightedNeighbor {
    RegionId region;
    double distance;
};

struct LandGraph {
    std::vector<std::vector<RegionId>> neighbors;
    std::vector<std::vector<WeightedNeighbor>> weightedNeighbors;
};

[[nodiscard]] double seedTraversalDistance(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    const std::vector<Region *> &regions,
    const ClaimCostParameters &parameters,
    RegionId first,
    RegionId second) {
    const auto firstIndex = static_cast<std::size_t>(first);
    const auto secondIndex = static_cast<std::size_t>(second);
    const auto &firstCell = division.cells[firstIndex];
    const auto &secondCell = division.cells[secondIndex];
    const auto &firstRegion = *regions[firstIndex];
    const auto &secondRegion = *regions[secondIndex];

    const auto siteDistance = (firstCell.sitePosition
                               - secondCell.sitePosition).length()
                              / parameters.averageCellLength;
    const auto worldDistance = normalizedDistance(boundingBox,
                                                  firstCell.sitePosition,
                                                  secondCell.sitePosition);
    const auto elevationPenalty = parameters.elevationContribution
                                  * std::abs(firstRegion.elevation()
                                             - secondRegion.elevation());
    const auto riverPenalty = sharedBoundaryHasRiver(
                                  firstCell,
                                  firstRegion,
                                  secondCell,
                                  secondRegion,
                                  parameters.sharedEdgeTolerance)
                                  ? parameters.riverContribution
                                  : 0.0;

    auto shortBorderPenalty = 0.0;
    if (parameters.shortBorderContribution > 0.0) {
        const auto borderLength = sharedBoundaryLength(
            firstCell,
            secondCell,
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
        if (!firstRegion.hasLandType() || !secondRegion.hasLandType()) {
            throw std::logic_error(
                "Province land-type distances require classified land regions.");
        }
        if (firstRegion.landType() != secondRegion.landType())
            landTypePenalty = parameters.landTypeContribution;
    }

    const auto distance = siteDistance
                          + parameters.distanceContribution * worldDistance
                          + elevationPenalty
                          + riverPenalty
                          + shortBorderPenalty
                          + landTypePenalty;
    if (!std::isfinite(distance) || distance < 0.0) {
        throw std::logic_error(
            "Province generation produced an invalid seed distance.");
    }
    return distance;
}

[[nodiscard]] LandGraph buildLandGraph(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    const std::vector<Region *> &regions,
    const ClaimCostParameters &parameters) {
    LandGraph graph{
        .neighbors = std::vector<std::vector<RegionId>>(regions.size()),
        .weightedNeighbors =
            std::vector<std::vector<WeightedNeighbor>>(regions.size()),
    };

    for (std::size_t region = 0; region < division.cells.size(); ++region) {
        for (const auto neighbor : division.cells[region].neighbors) {
            const auto neighborIndex = static_cast<std::size_t>(neighbor);
            if (neighborIndex >= regions.size()) {
                throw std::logic_error(
                    "Province generation received an invalid cell neighbor.");
            }
            if (neighborIndex == region
                || regions[region]->isWater()
                || regions[neighborIndex]->isWater()) {
                continue;
            }
            graph.neighbors[region].push_back(neighbor);
            graph.neighbors[neighborIndex].push_back(
                static_cast<RegionId>(region));
        }
    }

    for (auto &neighbors : graph.neighbors) {
        std::ranges::sort(neighbors);
        const auto afterUnique = std::ranges::unique(neighbors).begin();
        neighbors.erase(afterUnique, neighbors.end());
    }
    for (std::size_t region = 0; region < graph.neighbors.size(); ++region) {
        for (const auto neighbor : graph.neighbors[region]) {
            if (region >= static_cast<std::size_t>(neighbor))
                continue;
            const auto distance = seedTraversalDistance(
                boundingBox,
                division,
                regions,
                parameters,
                static_cast<RegionId>(region),
                neighbor);
            graph.weightedNeighbors[region].push_back({neighbor, distance});
            graph.weightedNeighbors[neighbor].push_back(
                {static_cast<RegionId>(region), distance});
        }
    }
    return graph;
}

[[nodiscard]] std::uint64_t mixSeed(std::uint64_t value) noexcept {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

struct DistanceEntry {
    double distance;
    RegionId region;
};

struct MoreDistantEntry {
    [[nodiscard]] bool operator()(const DistanceEntry &left,
                                  const DistanceEntry &right) const noexcept {
        const auto leftDistance = quantizedCost(left.distance);
        const auto rightDistance = quantizedCost(right.distance);
        if (leftDistance != rightDistance)
            return leftDistance > rightDistance;
        return left.region > right.region;
    }
};

void updateNearestSeedDistances(
    const LandGraph &graph,
    const std::vector<bool> &eligible,
    RegionId seed,
    std::vector<double> &nearestDistances) {
    std::priority_queue<DistanceEntry,
                        std::vector<DistanceEntry>,
                        MoreDistantEntry> frontier;
    nearestDistances[seed] = 0.0;
    frontier.push({0.0, seed});

    while (!frontier.empty()) {
        const auto current = frontier.top();
        frontier.pop();
        if (current.distance > nearestDistances[current.region] + EPS)
            continue;

        for (const auto &edge : graph.weightedNeighbors[current.region]) {
            if (!eligible[edge.region])
                continue;
            const auto candidateDistance = current.distance + edge.distance;
            if (candidateDistance + EPS >= nearestDistances[edge.region])
                continue;
            nearestDistances[edge.region] = candidateDistance;
            frontier.push({candidateDistance, edge.region});
        }
    }
}

[[nodiscard]] std::size_t maximumProvinceCapacity(
    double startScore,
    double baseCost,
    std::size_t maximumRegionCount) noexcept {
    auto capacity = maximumRegionCount == 0
                        ? std::numeric_limits<std::size_t>::max()
                        : maximumRegionCount;
    if (baseCost <= 0.0)
        return capacity;

    const auto affordableClaims = std::floor(
        (static_cast<long double>(startScore)
         + static_cast<long double>(EPS))
        / static_cast<long double>(baseCost));
    const auto maximumSize = static_cast<long double>(
        std::numeric_limits<std::size_t>::max());
    const auto budgetCapacity = affordableClaims >= maximumSize - 1.0L
                                    ? std::numeric_limits<std::size_t>::max()
                                    : static_cast<std::size_t>(affordableClaims)
                                          + 1;
    return std::min(capacity, budgetCapacity);
}

[[nodiscard]] std::vector<RegionId> selectProvinceSeeds(
    const LandGraph &graph,
    const std::vector<bool> &eligible,
    double minimumDistance,
    std::size_t provinceCapacity,
    std::uint64_t generationSeed) {
    std::vector<std::vector<RegionId>> components;
    std::vector<std::size_t> componentIds(
        eligible.size(),
        std::numeric_limits<std::size_t>::max());
    for (std::size_t first = 0; first < eligible.size(); ++first) {
        if (!eligible[first]
            || componentIds[first] != std::numeric_limits<std::size_t>::max()) {
            continue;
        }

        const auto componentId = components.size();
        components.emplace_back();
        std::vector<RegionId> pending{static_cast<RegionId>(first)};
        componentIds[first] = componentId;
        while (!pending.empty()) {
            const auto region = pending.back();
            pending.pop_back();
            components.back().push_back(region);
            for (const auto neighbor : graph.neighbors[region]) {
                if (!eligible[neighbor]
                    || componentIds[neighbor]
                           != std::numeric_limits<std::size_t>::max()) {
                    continue;
                }
                componentIds[neighbor] = componentId;
                pending.push_back(neighbor);
            }
        }
        std::ranges::sort(components.back());
    }

    if (components.empty())
        return {};

    std::vector<std::size_t> requiredSeedCounts(components.size(), 1);
    if (provinceCapacity != std::numeric_limits<std::size_t>::max()) {
        for (std::size_t component = 0;
             component < components.size();
             ++component) {
            requiredSeedCounts[component] =
                1 + (components[component].size() - 1)
                        / provinceCapacity;
        }
    }

    std::vector<bool> selected(eligible.size(), false);
    std::vector<double> nearestDistances(
        eligible.size(),
        std::numeric_limits<double>::infinity());
    std::vector<std::size_t> componentSeedCounts(components.size(), 0);
    std::vector<RegionId> seeds;
    const auto addSeed = [&](RegionId seed) {
        selected[seed] = true;
        ++componentSeedCounts[componentIds[seed]];
        seeds.push_back(seed);
        updateNearestSeedDistances(graph,
                                   eligible,
                                   seed,
                                   nearestDistances);
    };

    constexpr auto domain = 0x70726f76696e6365ULL;
    for (const auto &component : components) {
        auto firstSeed = component.front();
        auto firstRank = mixSeed(generationSeed
                                 ^ domain
                                 ^ static_cast<std::uint64_t>(firstSeed));
        for (const auto region : component) {
            const auto rank = mixSeed(generationSeed
                                      ^ domain
                                      ^ static_cast<std::uint64_t>(region));
            if (rank < firstRank
                || (rank == firstRank && region < firstSeed)) {
                firstSeed = region;
                firstRank = rank;
            }
        }
        addSeed(firstSeed);
    }

    while (true) {
        auto candidate = INVALID_REGION_ID;
        auto candidateDistanceOrder =
            -std::numeric_limits<long double>::infinity();
        for (std::size_t region = 0; region < eligible.size(); ++region) {
            if (!eligible[region] || selected[region])
                continue;
            const auto component = componentIds[region];
            const auto needsCapacity = componentSeedCounts[component]
                                       < requiredSeedCounts[component];
            const auto distanceOrder = quantizedCost(nearestDistances[region]);
            const auto isFarEnough = nearestDistances[region]
                                     > minimumDistance + EPS;
            if (!needsCapacity && !isFarEnough)
                continue;
            const auto regionId = static_cast<RegionId>(region);
            if (candidate == INVALID_REGION_ID
                || distanceOrder > candidateDistanceOrder
                || (distanceOrder == candidateDistanceOrder
                    && regionId < candidate)) {
                candidate = regionId;
                candidateDistanceOrder = distanceOrder;
            }
        }
        if (candidate == INVALID_REGION_ID)
            break;
        addSeed(candidate);
    }

    return seeds;
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
    std::size_t maximumRegionCount,
    double seedMinimumDistance,
    std::uint64_t generationSeed) {
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
        || landTypeContribution < 0.0
        || !std::isfinite(seedMinimumDistance)
        || seedMinimumDistance < 0.0) {
        throw std::invalid_argument(
            "Province scores, costs, contributions, and seed distance must be finite and non-negative.");
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
    const auto width = static_cast<long double>(boundingBox.max.x)
                       - static_cast<long double>(boundingBox.min.x);
    const auto height = static_cast<long double>(boundingBox.max.y)
                        - static_cast<long double>(boundingBox.min.y);
    const auto averageCellLength = static_cast<double>(std::sqrt(
        width * height
        / static_cast<long double>(division.cells.size())));
    if (!std::isfinite(averageCellLength) || averageCellLength <= 0.0) {
        throw std::logic_error(
            "Province generation could not determine an average cell length.");
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
    const auto landGraph = buildLandGraph(boundingBox,
                                          division,
                                          indexedRegions,
                                          costParameters);
    const auto provinceCapacity = maximumProvinceCapacity(startScore,
                                                          baseCost,
                                                          maximumRegionCount);
    std::vector<bool> assigned(regions.size(), false);
    std::vector<bool> eligible(regions.size(), false);
    auto unassignedLandCount = std::size_t{0};
    for (std::size_t region = 0; region < indexedRegions.size(); ++region) {
        assigned[region] = indexedRegions[region]->isWater();
        eligible[region] = !assigned[region];
        if (eligible[region])
            ++unassignedLandCount;
    }

    std::vector<RegionId> provinceSeeds;
    std::vector<std::vector<RegionId>> provinceRegions;
    std::vector<double> remainingScores;
    std::vector<double> growthDistances(
        regions.size(),
        std::numeric_limits<double>::infinity());
    provinceSeeds.reserve(regions.size());
    provinceRegions.reserve(regions.size());
    remainingScores.reserve(regions.size());

    using ClaimQueue = std::priority_queue<Claim,
                                           std::vector<Claim>,
                                           MoreExpensiveClaim>;
    const auto addFrontier = [&](ProvinceId province,
                                 RegionId fromRegion,
                                 ClaimQueue &frontier) {
        const auto seed = provinceSeeds[province];
        for (const auto neighbor : landGraph.neighbors[fromRegion]) {
            if (assigned[neighbor])
                continue;
            const auto cost = provinceClaimCost(
                boundingBox,
                division,
                indexedRegions,
                costParameters,
                seed,
                fromRegion,
                neighbor);
            const auto distanceFromSeed = growthDistances[fromRegion] + cost;
            if (!std::isfinite(distanceFromSeed)) {
                throw std::logic_error(
                    "Province generation produced an invalid growth distance.");
            }
            frontier.push({distanceFromSeed,
                           cost,
                           province,
                           seed,
                           neighbor,
                           fromRegion});
        }
    };
    const auto addSeedBatch = [&](const std::vector<RegionId> &seeds,
                                  ClaimQueue &frontier) {
        const auto firstProvince = provinceSeeds.size();
        for (const auto seed : seeds) {
            if (assigned[seed]) {
                throw std::logic_error(
                    "Province generation selected an assigned seed region.");
            }
            if (provinceSeeds.size() >= INVALID_PROVINCE_ID) {
                throw std::logic_error(
                    "Province generation exceeded the province ID range.");
            }
            const auto province = static_cast<ProvinceId>(
                provinceSeeds.size());
            provinceSeeds.push_back(seed);
            provinceRegions.push_back({seed});
            remainingScores.push_back(startScore);
            assigned[seed] = true;
            eligible[seed] = false;
            growthDistances[seed] = 0.0;
            --unassignedLandCount;
            indexedRegions[seed]->setProvinceId(province);
        }
        for (auto province = firstProvince;
             province < provinceSeeds.size();
             ++province) {
            addFrontier(static_cast<ProvinceId>(province),
                        provinceSeeds[province],
                        frontier);
        }
    };
    const auto growSeedBatch = [&](ClaimQueue &frontier) {
        while (!frontier.empty()) {
            const auto claim = frontier.top();
            frontier.pop();
            if (assigned[claim.region])
                continue;
            if (maximumRegionCount != 0
                && provinceRegions[claim.province].size()
                       >= maximumRegionCount) {
                continue;
            }
            if (claim.cost > remainingScores[claim.province] + EPS)
                continue;

            remainingScores[claim.province] = std::max(
                0.0,
                remainingScores[claim.province] - claim.cost);
            assigned[claim.region] = true;
            eligible[claim.region] = false;
            growthDistances[claim.region] = claim.distanceFromSeed;
            --unassignedLandCount;
            indexedRegions[claim.region]->setProvinceId(claim.province);
            provinceRegions[claim.province].push_back(claim.region);
            addFrontier(claim.province, claim.region, frontier);
        }
    };

    auto seedRound = std::uint64_t{0};
    while (unassignedLandCount > 0) {
        const auto seeds = selectProvinceSeeds(
            landGraph,
            eligible,
            seedMinimumDistance,
            provinceCapacity,
            generationSeed ^ mixSeed(seedRound));
        if (seeds.empty()) {
            throw std::logic_error(
                "Province generation could not select an unassigned land seed.");
        }
        const auto countBeforeGrowth = unassignedLandCount;
        ClaimQueue frontier;
        addSeedBatch(seeds, frontier);
        growSeedBatch(frontier);
        if (unassignedLandCount >= countBeforeGrowth) {
            throw std::logic_error(
                "Province generation seed growth made no progress.");
        }
        ++seedRound;
    }

    std::vector<Province> provinces;
    provinces.reserve(provinceSeeds.size());
    for (std::size_t province = 0;
         province < provinceSeeds.size();
         ++province) {
        provinces.emplace_back(provinceSeeds[province],
                               std::move(provinceRegions[province]),
                               remainingScores[province]);
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
