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
    Swamp = 7,
    Rainforest = 8,
    Tundra = 9,
};

struct LandTypeConditions {
    double snowTemperature{0.0};
    double coldTemperature{6.0};
    double hotTemperature{22.0};

    double dryHumidity{0.3};
    double wetHumidity{0.7};

    double sparseVegetation{0.35};
    double lushVegetation{0.6};

    double lowlandElevation{0.15};
    double hillElevation{0.4};
    double mountainElevation{0.7};
};

[[nodiscard]] bool isValidLandType(LandType landType) noexcept;
void validateLandTypeConditions(const LandTypeConditions &conditions);

[[nodiscard]] LandType classifyLandType(
    double elevation,
    double seaLevel,
    const climate::ClimateSample &climate,
    const LandTypeConditions &conditions = {});

} // namespace worldgen
