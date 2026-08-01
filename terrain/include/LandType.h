#pragma once

#include "ClimateGenerator.h"

#include <cstdint>

namespace worldgen {

enum class LandType : std::uint8_t {
    Mountain = 0,
    SnowPeaks = 1,
    Hills = 2,
    Fields = 3,
    Forest = 4,
    Sparse = 5,
    Desert = 6,
    Beach = 7,
    Swamp = 8,
    Rainforest = 9,
    Tundra = 10,
};

[[nodiscard]] bool isValidLandType(LandType landType) noexcept;

[[nodiscard]] LandType classifyLandType(
    double elevation,
    double seaLevel,
    const climate::ClimateSample &climate);

} // namespace worldgen
