#include "LandType.h"

#include <cmath>
#include <stdexcept>

namespace worldgen {

namespace {

constexpr double snowPeakElevation = 0.7;
constexpr double mountainElevation = 0.7;
constexpr double hillElevation = 0.4;
constexpr double swampElevation = 0.12;
constexpr double beachElevation = 0.06;

constexpr double snowTemperature = 0.0;
constexpr double tundraTemperature = 5.0;
constexpr double hotTemperature = 20.0;

constexpr double wetHumidity = 0.7;
constexpr double forestHumidity = 0.4;
constexpr double dryHumidity = 0.3;
constexpr double desertHumidity = 0.25;

constexpr double lushVegetation = 0.7;
constexpr double forestVegetation = 0.55;
constexpr double sparseVegetation = 0.3;
constexpr double swampVegetation = 0.5;

[[nodiscard]] double normalizedLandElevation(double elevation,
                                             double seaLevel) noexcept {
    if (seaLevel >= 1.0)
        return 0.0;
    return (elevation - seaLevel) / (1.0 - seaLevel);
}

void validateInputs(double elevation,
                    double seaLevel,
                    const climate::ClimateSample &climate) {
    if (!std::isfinite(elevation) || elevation < 0.0 || elevation > 1.0)
        throw std::invalid_argument("Land elevation must be between zero and one.");
    if (!std::isfinite(seaLevel) || seaLevel < 0.0 || seaLevel > 1.0)
        throw std::invalid_argument("Land sea level must be between zero and one.");
    if (elevation < seaLevel)
        throw std::invalid_argument("A land type requires elevation at or above sea level.");
    if (!std::isfinite(climate.temperature)
        || climate.temperature < -50.0 || climate.temperature > 50.0) {
        throw std::invalid_argument(
            "Land temperature must be between -50 and 50.");
    }
    if (!std::isfinite(climate.humidity)
        || climate.humidity < 0.0 || climate.humidity > 1.0) {
        throw std::invalid_argument(
            "Land humidity must be between zero and one.");
    }
    if (!std::isfinite(climate.vegetation)
        || climate.vegetation < 0.0 || climate.vegetation > 1.0) {
        throw std::invalid_argument(
            "Land vegetation must be between zero and one.");
    }
}

} // namespace

bool isValidLandType(LandType landType) noexcept {
    switch (landType) {
    case LandType::Mountain:
    case LandType::SnowPeaks:
    case LandType::Hills:
    case LandType::Fields:
    case LandType::Forest:
    case LandType::Sparse:
    case LandType::Desert:
    case LandType::Beach:
    case LandType::Swamp:
    case LandType::Rainforest:
    case LandType::Tundra:
        return true;
    }
    return false;
}

LandType classifyLandType(double elevation,
                          double seaLevel,
                          const climate::ClimateSample &climate) {
    validateInputs(elevation, seaLevel, climate);
    const auto landElevation = normalizedLandElevation(elevation, seaLevel);

    if (landElevation >= snowPeakElevation
        && climate.temperature <= snowTemperature) {
        return LandType::SnowPeaks;
    }
    if (landElevation >= mountainElevation)
        return LandType::Mountain;
    if (climate.temperature <= tundraTemperature)
        return LandType::Tundra;
    if (landElevation >= hillElevation)
        return LandType::Hills;
    if (landElevation <= swampElevation
        && climate.humidity >= wetHumidity
        && climate.vegetation >= swampVegetation) {
        return LandType::Swamp;
    }
    if (landElevation <= beachElevation)
        return LandType::Beach;
    if (climate.temperature >= hotTemperature
        && climate.humidity >= wetHumidity
        && climate.vegetation >= lushVegetation) {
        return LandType::Rainforest;
    }
    if (climate.temperature >= hotTemperature
        && climate.humidity <= desertHumidity
        && climate.vegetation <= sparseVegetation) {
        return LandType::Desert;
    }
    if (climate.humidity >= forestHumidity
        && climate.vegetation >= forestVegetation) {
        return LandType::Forest;
    }
    if (climate.humidity <= dryHumidity
        || climate.vegetation <= sparseVegetation) {
        return LandType::Sparse;
    }
    return LandType::Fields;
}

} // namespace worldgen
