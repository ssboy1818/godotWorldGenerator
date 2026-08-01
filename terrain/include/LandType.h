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
    double hotTemperature{20.0};

    double dryHumidity{0.45};
    double wetHumidity{0.62};

    double sparseVegetation{0.45};
    double lushVegetation{0.54};

    double lowlandElevation{0.18};
    double hillElevation{0.38};
    double mountainElevation{0.68};
};

[[nodiscard]] bool isValidLandType(LandType landType) noexcept;
void validateLandTypeConditions(const LandTypeConditions &conditions);

[[nodiscard]] LandType classifyLandType(
    double normalizedElevation,
    const climate::ClimateSample &climate,
    const LandTypeConditions &conditions = {});

} // namespace worldgen
