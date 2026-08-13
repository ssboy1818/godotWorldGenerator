#pragma once

#include "ClimateGenerator.h"

#include <cstdint>

namespace worldgen {

enum class LandType : std::uint8_t {
    Tundra = 0,
    BorealForest = 1,
    Grassland = 2,
    TemperateForest = 3,
    Steppe = 4,
    Wetland = 5,
    Desert = 6,
    Savanna = 7,
    TropicalForest = 8,
    Rainforest = 9,
};

struct LandTypeConditions {
    double polarTemperature{0.0};
    double coldTemperature{6.0};
    double hotTemperature{20.0};

    double dryHumidity{0.45};
    double wetHumidity{0.62};

    double sparseVegetation{0.45};
    double lushVegetation{0.54};

    double wetlandElevation{0.18};
};

[[nodiscard]] bool isValidLandType(LandType landType) noexcept;
void validateLandTypeConditions(const LandTypeConditions &conditions);

[[nodiscard]] LandType classifyLandType(
    double normalizedElevation,
    const climate::ClimateSample &climate,
    const LandTypeConditions &conditions = {});

} // namespace worldgen
