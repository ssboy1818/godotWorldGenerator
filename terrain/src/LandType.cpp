#include "LandType.h"

#include <cmath>
#include <stdexcept>

namespace worldgen {

namespace {

void validateInputs(double normalizedElevation,
                    const climate::ClimateSample &climate) {
    if (!std::isfinite(normalizedElevation)
        || normalizedElevation < 0.0 || normalizedElevation > 1.0) {
        throw std::invalid_argument(
            "Normalized land elevation must be between zero and one.");
    }
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
    case LandType::Swamp:
    case LandType::Rainforest:
    case LandType::Tundra:
        return true;
    }
    return false;
}

void validateLandTypeConditions(const LandTypeConditions &conditions) {
    const auto validTemperature = [](double value) {
        return std::isfinite(value) && value >= -50.0 && value <= 50.0;
    };
    if (!validTemperature(conditions.snowTemperature)
        || !validTemperature(conditions.coldTemperature)
        || !validTemperature(conditions.hotTemperature)
        || conditions.snowTemperature > conditions.coldTemperature
        || conditions.coldTemperature >= conditions.hotTemperature) {
        throw std::invalid_argument(
            "Land type temperatures must satisfy -50 <= snow <= cold < hot <= 50.");
    }

    const auto validRatio = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 1.0;
    };
    if (!validRatio(conditions.dryHumidity)
        || !validRatio(conditions.wetHumidity)
        || conditions.dryHumidity >= conditions.wetHumidity) {
        throw std::invalid_argument(
            "Land type humidity must satisfy 0 <= dry < wet <= 1.");
    }
    if (!validRatio(conditions.sparseVegetation)
        || !validRatio(conditions.lushVegetation)
        || conditions.sparseVegetation >= conditions.lushVegetation) {
        throw std::invalid_argument(
            "Land type vegetation must satisfy 0 <= sparse < lush <= 1.");
    }
    if (!validRatio(conditions.lowlandElevation)
        || !validRatio(conditions.hillElevation)
        || !validRatio(conditions.mountainElevation)
        || conditions.lowlandElevation >= conditions.hillElevation
        || conditions.hillElevation >= conditions.mountainElevation) {
        throw std::invalid_argument(
            "Land type elevations must satisfy 0 <= lowland < hill < mountain <= 1.");
    }
}

LandType classifyLandType(double normalizedElevation,
                          const climate::ClimateSample &climate,
                          const LandTypeConditions &conditions) {
    validateInputs(normalizedElevation, climate);
    validateLandTypeConditions(conditions);

    if (normalizedElevation >= conditions.mountainElevation
        && climate.temperature <= conditions.snowTemperature) {
        return LandType::SnowPeaks;
    }
    if (normalizedElevation >= conditions.mountainElevation
        && climate.temperature > conditions.snowTemperature) {
        return LandType::Mountain;
    }
    if (climate.temperature <= conditions.coldTemperature
        && climate.vegetation <= conditions.sparseVegetation) {
        return LandType::Tundra;
    }
    if (normalizedElevation >= conditions.hillElevation
        && climate.temperature > conditions.coldTemperature) {
        return LandType::Hills;
    }
    if (normalizedElevation <= conditions.lowlandElevation
        && climate.temperature > conditions.coldTemperature
        && climate.humidity >= conditions.wetHumidity
        && climate.vegetation >= conditions.lushVegetation) {
        return LandType::Swamp;
    }
    if (climate.temperature >= conditions.hotTemperature
        && climate.humidity >= conditions.wetHumidity
        && climate.vegetation >= conditions.lushVegetation) {
        return LandType::Rainforest;
    }
    if (climate.temperature >= conditions.hotTemperature
        && climate.humidity <= conditions.dryHumidity
        && climate.vegetation <= conditions.sparseVegetation) {
        return LandType::Desert;
    }
    if (climate.temperature > conditions.coldTemperature
        && climate.humidity > conditions.dryHumidity
        && climate.vegetation >= conditions.lushVegetation) {
        return LandType::Forest;
    }
    if (climate.humidity <= conditions.dryHumidity
        && climate.vegetation <= conditions.sparseVegetation) {
        return LandType::Sparse;
    }
    return LandType::Fields;
}

} // namespace worldgen
