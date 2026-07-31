#include "PerlinNoise.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace worldgen {

namespace {

struct Gradient {
    double x;
    double y;
};

constexpr std::array<Gradient, 8> gradients{{
    {1.0, 0.0},
    {-1.0, 0.0},
    {0.0, 1.0},
    {0.0, -1.0},
    {0.7071067811865475244, 0.7071067811865475244},
    {-0.7071067811865475244, 0.7071067811865475244},
    {0.7071067811865475244, -0.7071067811865475244},
    {-0.7071067811865475244, -0.7071067811865475244},
}};

constexpr std::uint64_t mix(std::uint64_t value) noexcept {
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

std::uint64_t latticeHash(std::int64_t x,
                          std::int64_t y,
                          std::uint64_t seed) noexcept {
    const auto xBits = static_cast<std::uint64_t>(x);
    const auto yBits = static_cast<std::uint64_t>(y);
    return mix(xBits ^ (mix(yBits) + mix(seed) + 0x9e3779b97f4a7c15ULL));
}

constexpr double fade(double value) noexcept {
    return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
}

constexpr double lerp(double from, double to, double amount) noexcept {
    return from + amount * (to - from);
}

double gradientDot(std::int64_t x,
                   std::int64_t y,
                   double offsetX,
                   double offsetY,
                   std::uint64_t seed) noexcept {
    const auto &gradient = gradients[latticeHash(x, y, seed) % gradients.size()];
    return gradient.x * offsetX + gradient.y * offsetY;
}

} // namespace

namespace noise {

double perlinNoise(Vector2d pos, std::uint64_t seed, bool normalized) noexcept {
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y))
        return std::numeric_limits<double>::quiet_NaN();

    const auto minimumCoordinate = static_cast<double>(std::numeric_limits<std::int64_t>::min());
    const auto maximumCoordinate = -minimumCoordinate;
    if (pos.x < minimumCoordinate || pos.x >= maximumCoordinate
        || pos.y < minimumCoordinate || pos.y >= maximumCoordinate) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const auto cellX = static_cast<std::int64_t>(std::floor(pos.x));
    const auto cellY = static_cast<std::int64_t>(std::floor(pos.y));
    const auto localX = pos.x - static_cast<double>(cellX);
    const auto localY = pos.y - static_cast<double>(cellY);

    const auto bottomLeft = gradientDot(cellX, cellY, localX, localY, seed);
    const auto bottomRight = gradientDot(cellX + 1, cellY, localX - 1.0, localY, seed);
    const auto topLeft = gradientDot(cellX, cellY + 1, localX, localY - 1.0, seed);
    const auto topRight = gradientDot(cellX + 1, cellY + 1, localX - 1.0, localY - 1.0, seed);

    const auto horizontal = fade(localX);
    const auto vertical = fade(localY);
    const auto value = lerp(lerp(bottomLeft, bottomRight, horizontal),
                            lerp(topLeft, topRight, horizontal),
                            vertical);
    if (!normalized)
        return value;

    return std::clamp((value + 1.0) * 0.5, 0.0, 1.0);
}

double edgeDecay(const BoundingBox &worldBounds,
                 Vector2d decayRadius,
                 Vector2d pos) noexcept {
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y)
        || !std::isfinite(decayRadius.x) || !std::isfinite(decayRadius.y)
        || decayRadius.x <= 0.0 || decayRadius.y <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const auto distanceX = std::min(pos.x - worldBounds.min.x, worldBounds.max.x - pos.x);
    const auto distanceY = std::min(pos.y - worldBounds.min.y, worldBounds.max.y - pos.y);
    const auto xMask = std::clamp(1.0 - distanceX / decayRadius.x, 0.0, 1.0);
    const auto yMask = std::clamp(1.0 - distanceY / decayRadius.y, 0.0, 1.0);
    return std::max(xMask, yMask);
}

double noise(const BoundingBox &worldBounds,
             Vector2d pos,
             Vector2d decayRadius,
             std::uint64_t seed,
             std::uint32_t octaves,
             double baseFrequency,
             double frequencyCoefficient,
             double strengthCoefficient) noexcept {
    if (octaves == 0
        || !std::isfinite(baseFrequency) || baseFrequency <= 0.0
        || !std::isfinite(frequencyCoefficient) || frequencyCoefficient <= 0.0
        || !std::isfinite(strengthCoefficient) || strengthCoefficient < 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto frequency = baseFrequency;
    auto strength = 1.0;
    auto totalStrength = 0.0;
    auto value = 0.0;

    for (std::uint32_t octave = 0; octave < octaves; ++octave) {
        if (!std::isfinite(frequency) || !std::isfinite(strength))
            return std::numeric_limits<double>::quiet_NaN();

        value += perlinNoise(pos * frequency, seed, true) * strength;
        totalStrength += strength;
        frequency *= frequencyCoefficient;
        strength *= strengthCoefficient;
    }

    return std::lerp(value / totalStrength,
                     0.0,
                     edgeDecay(worldBounds, decayRadius, pos));
}

} // namespace noise

} // namespace worldgen
