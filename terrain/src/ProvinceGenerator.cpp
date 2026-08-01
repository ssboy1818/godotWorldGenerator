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

[[nodiscard]] bool segmentsShareBoundary(Vector2d firstStart,
                                         Vector2d firstEnd,
                                         Vector2d secondStart,
                                         Vector2d secondEnd,
                                         double tolerance) noexcept {
    const auto firstDirection = firstEnd - firstStart;
    const auto secondDirection = secondEnd - secondStart;
    const auto firstLength = firstDirection.length();
    const auto secondLength = secondDirection.length();
    if (firstLength <= tolerance || secondLength <= tolerance)
        return false;

    const Vector2d unit{firstDirection.x / firstLength,
                        firstDirection.y / firstLength};
    const auto perpendicularDistance = [&](Vector2d point) {
        const auto relative = point - firstStart;
        return std::abs(relative.x * unit.y - relative.y * unit.x);
    };
    if (perpendicularDistance(secondStart) > tolerance
        || perpendicularDistance(secondEnd) > tolerance) {
        return false;
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
    return overlap > tolerance;
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
            if (segmentsShareBoundary(riverCell.vertices[edge],
                                      riverCell.vertices[riverNext],
                                      otherCell.vertices[otherEdge],
                                      otherCell.vertices[otherNext],
                                      tolerance)) {
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

} // namespace

std::vector<Province> generateProvinces(
    const BoundingBox &boundingBox,
    const WorldDivision &division,
    std::span<Region> regions,
    double startScore,
    double riverContribution,
    double elevationContribution,
    double distanceContribution,
    double baseCost) {
    if (division.cells.empty())
        return {};

    const auto indexedRegions = indexRegions(division, regions);
    const auto sharedEdgeTolerance = numericalToleranceFor(boundingBox)
                                         .sharedEdgeLength();
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
        const auto provinceCenter = division.cells[seedIndex].sitePosition;
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
            const auto &from = *indexedRegions[fromIndex];
            for (const auto neighborCell : fromCell.neighbors) {
                const auto neighborIndex = static_cast<std::size_t>(neighborCell);
                if (neighborIndex >= indexedRegions.size()) {
                    throw std::logic_error(
                        "Province generation received an invalid cell neighbor.");
                }
                if (assigned[neighborIndex])
                    continue;

                const auto &neighbor = *indexedRegions[neighborIndex];
                const auto crossesRiver = sharedBoundaryHasRiver(
                    fromCell,
                    from,
                    division.cells[neighborIndex],
                    neighbor,
                    sharedEdgeTolerance);
                const auto elevationDifference = std::abs(
                    from.elevation() - neighbor.elevation());
                const auto distance = normalizedDistance(
                    boundingBox,
                    provinceCenter,
                    division.cells[neighborIndex].sitePosition);
                const auto cost = baseCost
                                  + elevationContribution * elevationDifference
                                  + distanceContribution * distance
                                  + (crossesRiver ? riverContribution : 0.0);
                if (!std::isfinite(cost) || cost < 0.0) {
                    throw std::logic_error(
                        "Province generation produced an invalid claim cost.");
                }
                frontier.push({cost,
                               static_cast<RegionId>(neighborIndex),
                               fromRegion});
            }
        };

        addFrontier(seed);
        while (!frontier.empty()) {
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

    for (const auto *region : indexedRegions) {
        if (region->isLand() != region->hasProvince()) {
            throw std::logic_error(
                "Province generation did not assign exactly the land regions.");
        }
    }

    return provinces;
}

} // namespace worldgen
