#pragma once

#include "BoundingBox.h"
#include "Vector2.h"

#include <cstdint>

namespace noise {

static std::uint64_t seed = 0;

[[nodiscard]] double perlinNoise(Vector2d pos, bool normalized = false) noexcept;

[[nodiscard]] double edgeDecay(const BoundingBox &worldBounds,
                               Vector2d decayRadius,
                               double x,
                               double y) noexcept;

};