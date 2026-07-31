#include "SiteValidation.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

struct GridCell {
    std::int64_t x{0};
    std::int64_t y{0};

    bool operator==(const GridCell &) const noexcept = default;
};

struct GridCellHash {
    std::size_t operator()(GridCell cell) const noexcept {
        const auto x = static_cast<std::uint64_t>(cell.x);
        const auto y = static_cast<std::uint64_t>(cell.y);
        auto value = x + 0x9e3779b97f4a7c15ULL;
        value ^= y + 0x9e3779b97f4a7c15ULL + (value << 6U) + (value >> 2U);
        return static_cast<std::size_t>(value);
    }
};

GridCell gridCellFor(Vector2d point,
                     const BoundingBox &boundingBox,
                     double cellSize) noexcept {
    return {
        static_cast<std::int64_t>(
            std::floor((point.x - boundingBox.min.x) / cellSize)),
        static_cast<std::int64_t>(
            std::floor((point.y - boundingBox.min.y) / cellSize)),
    };
}

[[noreturn]] void throwSitesTooClose(std::size_t first,
                                     std::size_t second,
                                     Vector2d firstPosition,
                                     Vector2d secondPosition,
                                     double tolerance) {
    std::ostringstream message;
    message << std::setprecision(17)
            << "Sites " << first << " (" << firstPosition.x << ", "
            << firstPosition.y << ") and " << second << " ("
            << secondPosition.x << ", " << secondPosition.y
            << ") are identical or too close; their separation must be greater than "
            << tolerance << " for this bounding box.";
    throw std::invalid_argument(message.str());
}

} // namespace

NumericalTolerance validateSites(std::span<const Site> sites,
                                 const BoundingBox &boundingBox) {
    if (sites.size() > std::numeric_limits<CellId>::max())
        throw std::invalid_argument("The number of sites exceeds the CellId range.");

    const auto tolerance = numericalToleranceFor(boundingBox);
    std::unordered_map<GridCell, std::vector<std::size_t>, GridCellHash> grid;
    grid.reserve(sites.size());

    for (std::size_t index = 0; index < sites.size(); ++index) {
        const auto position = sites[index].position;
        if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
            throw std::invalid_argument(
                "Every site must have finite coordinates; site "
                + std::to_string(index) + " is not finite.");
        }
        if (!boundingBox.contains(position)) {
            throw std::invalid_argument(
                "Every site must lie inside the bounding box; site "
                + std::to_string(index) + " lies outside it.");
        }

        const auto cell = gridCellFor(position,
                                      boundingBox,
                                      tolerance.siteSeparation);
        for (std::int64_t yOffset = -1; yOffset <= 1; ++yOffset) {
            for (std::int64_t xOffset = -1; xOffset <= 1; ++xOffset) {
                const auto bucket = grid.find(
                    {cell.x + xOffset, cell.y + yOffset});
                if (bucket == grid.end())
                    continue;

                for (const auto otherIndex : bucket->second) {
                    if (pointsAlmostEqual(position,
                                          sites[otherIndex].position,
                                          tolerance.siteSeparation)) {
                        throwSitesTooClose(otherIndex,
                                           index,
                                           sites[otherIndex].position,
                                           position,
                                           tolerance.siteSeparation);
                    }
                }
            }
        }

        grid[cell].push_back(index);
    }

    return tolerance;
}
