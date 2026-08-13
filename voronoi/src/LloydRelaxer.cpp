#include "LloydRelaxer.h"

#include "Fortune.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace worldgen {

namespace {

Vector2d polygonCentroid(const Cell &cell,
                         const BoundingBox &boundingBox) {
    if (cell.vertices.size() < 3) {
        throw std::logic_error(
            "Lloyd relaxation requires cells with at least three vertices.");
    }

    const auto width = boundingBox.max.x - boundingBox.min.x;
    const auto height = boundingBox.max.y - boundingBox.min.y;
    const auto normalized = [&](Vector2d point) {
        return Vector2d{
            (point.x - boundingBox.min.x) / width,
            (point.y - boundingBox.min.y) / height,
        };
    };

    // A fan around the first vertex avoids cancellation between a polygon far
    // from the coordinate origin and keeps all intermediate values near unity.
    const auto origin = normalized(cell.vertices.front());
    auto twiceArea = 0.0;
    Vector2d weightedCentroid;
    for (std::size_t index = 1; index + 1 < cell.vertices.size(); ++index) {
        const auto first = normalized(cell.vertices[index]) - origin;
        const auto second = normalized(cell.vertices[index + 1]) - origin;
        const auto cross = first.x * second.y - first.y * second.x;
        twiceArea += cross;
        weightedCentroid += (first + second) * cross;
    }

    if (twiceArea == 0.0 || !std::isfinite(twiceArea)) {
        throw std::logic_error(
            "Lloyd relaxation received a cell with invalid area.");
    }

    auto centroid = origin + weightedCentroid / (3.0 * twiceArea);
    if (!std::isfinite(centroid.x) || !std::isfinite(centroid.y)) {
        throw std::logic_error(
            "Lloyd relaxation produced a non-finite cell centroid.");
    }

    centroid.x = std::clamp(centroid.x, 0.0, 1.0);
    centroid.y = std::clamp(centroid.y, 0.0, 1.0);
    return {
        boundingBox.min.x + centroid.x * width,
        boundingBox.min.y + centroid.y * height,
    };
}

} // namespace

WorldDivision LloydRelaxer::relax(std::span<const Site> sites,
                                  const BoundingBox &boundingBox,
                                  std::size_t iterations) const {
    std::vector<Site> relaxedSites;
    relaxedSites.reserve(sites.size());
    for (const auto &site : sites)
        relaxedSites.emplace_back(site.position);

    const Fortune fortune;
    auto division = fortune.generate(relaxedSites, boundingBox);
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        if (division.cells.size() != relaxedSites.size()) {
            throw std::logic_error(
                "Lloyd relaxation received inconsistent Voronoi output.");
        }

        std::vector<Site> nextSites;
        nextSites.reserve(relaxedSites.size());
        for (std::size_t index = 0; index < division.cells.size(); ++index) {
            const auto &cell = division.cells[index];
            if (static_cast<std::size_t>(cell.id) != index) {
                throw std::logic_error(
                    "Lloyd relaxation requires stable cell IDs.");
            }
            nextSites.emplace_back(polygonCentroid(cell, boundingBox));
        }

        relaxedSites = std::move(nextSites);
        division = fortune.generate(relaxedSites, boundingBox);
    }

    return division;
}

} // namespace worldgen
