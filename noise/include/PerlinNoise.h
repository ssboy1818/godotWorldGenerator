#pragma once

#include "BoundingBox.h"
#include "Vector2.h"

#include <cstdint>

namespace noise {

static std::uint64_t seed = 0;

/// Returns deterministic, continuous 2D Perlin noise for a world-space position.
/// The result is in [-1.0, 1.0], or in [0.0, 1.0] when normalized is true.
[[nodiscard]] double perlinNoise(Vector2d pos, bool normalized = false) noexcept;

/// Returns a mask that is strongest at the world bounds and fades linearly to zero.
/// decayRadius.x and decayRadius.y control the fade width for their respective axes.
[[nodiscard]] double egdeDecay(const BoundingBox &worldBounds,
                               Vector2d decayRadius,
                               double x,
                               double y) noexcept;

/// Correctly spelled alias for egdeDecay.
[[nodiscard]] double edgeDecay(const BoundingBox &worldBounds,
                               Vector2d decayRadius,
                               double x,
                               double y) noexcept;

};