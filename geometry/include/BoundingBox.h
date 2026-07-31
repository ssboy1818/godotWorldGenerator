#pragma once

#include "Vector2.h"

#include <cmath>
#include <stdexcept>

namespace worldgen {

class BoundingBox {
public:
    Vector2d min;
    Vector2d max;

public:
    BoundingBox(Vector2d min_, Vector2d max_)
        : min(min_), max(max_) {
        if (!std::isfinite(min.x) || !std::isfinite(min.y)
            || !std::isfinite(max.x) || !std::isfinite(max.y)) {
            throw std::invalid_argument("A bounding box must have finite coordinates.");
        }
        if (min.x >= max.x || min.y >= max.y)
            throw std::invalid_argument("A bounding box must have positive width and height.");
        if (!std::isfinite(max.x - min.x) || !std::isfinite(max.y - min.y)) {
            throw std::invalid_argument("A bounding box must have finite dimensions.");
        }
    }

    [[nodiscard]] bool contains(Vector2d point) const noexcept {
        return point.x >= min.x && point.x <= max.x
            && point.y >= min.y && point.y <= max.y;
    }
};

} // namespace worldgen
