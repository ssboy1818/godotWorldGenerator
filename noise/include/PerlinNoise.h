#pragma once

#include "BoundingBox.h"
#include "Vector2.h"

#include <cstdint>

namespace worldgen {
namespace noise {

[[nodiscard]] double perlinNoise(Vector2d pos,
                                 std::uint64_t seed,
                                 bool normalized = false) noexcept;

[[nodiscard]] double edgeDecay(const BoundingBox &worldBounds,
                               Vector2d decayRadius,
                               Vector2d pos) noexcept;

[[nodiscard]] double noise(const BoundingBox &worldBounds,
                           Vector2d pos,
                           Vector2d decayRadius,
                           std::uint64_t seed,
                           std::uint32_t octaves = 4,
                           double baseFrequency = 0.01,
                           double frequencyCoefficient = 2.0,
                           double strengthCoefficient = 0.5) noexcept;

} // namespace noise
} // namespace worldgen
