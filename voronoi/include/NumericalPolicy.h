#pragma once

#include "BoundingBox.h"

#include <algorithm>
#include <cmath>
#include <limits>

struct NumericalTolerance {
    // Minimum supported separation between input sites.
    double siteSeparation{0.0};

    // Smaller tolerance used only to absorb clipping roundoff in output geometry.
    double geometryLength{0.0};

    [[nodiscard]] double sharedEdgeLength() const noexcept {
        // Polygon endpoints can accumulate error from several clipping operations.
        return geometryLength * 8.0;
    }
};

inline NumericalTolerance numericalToleranceFor(
    const BoundingBox &boundingBox) noexcept {
    constexpr double relativeBoxTolerance = 1e-10;
    constexpr double relativeGeometryTolerance = 1e-12;
    constexpr double coordinateUlpAllowance = 64.0;

    const auto width = boundingBox.max.x - boundingBox.min.x;
    const auto height = boundingBox.max.y - boundingBox.min.y;
    const auto extent = std::max(width, height);
    const auto coordinateScale = std::max({
        std::abs(boundingBox.min.x),
        std::abs(boundingBox.min.y),
        std::abs(boundingBox.max.x),
        std::abs(boundingBox.max.y),
        extent,
    });

    const auto floatingPointFloor = std::max(
        coordinateScale * std::numeric_limits<double>::epsilon()
            * coordinateUlpAllowance,
        std::numeric_limits<double>::denorm_min());
    return {
        std::max(extent * relativeBoxTolerance, floatingPointFloor),
        std::max(extent * relativeGeometryTolerance, floatingPointFloor),
    };
}

inline bool almostEqual(double left,
                        double right,
                        double tolerance) noexcept {
    return std::abs(left - right) <= tolerance;
}

inline bool pointsAlmostEqual(Vector2d left,
                              Vector2d right,
                              double tolerance) noexcept {
    return std::hypot(left.x - right.x, left.y - right.y) <= tolerance;
}
