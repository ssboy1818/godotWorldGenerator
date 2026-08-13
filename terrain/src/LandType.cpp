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
    case LandType::Tundra:
    case LandType::BorealForest:
    case LandType::Grassland:
    case LandType::TemperateForest:
    case LandType::Steppe:
    case LandType::Wetland:
    case LandType::Desert:
    case LandType::Savanna:
    case LandType::TropicalForest:
    case LandType::Rainforest:
        return true;
    }
    return false;
}

void validateLandTypeConditions(const LandTypeConditions &conditions) {
    const auto validTemperature = [](double value) {
        return std::isfinite(value) && value >= -50.0 && value <= 50.0;
    };
    if (!validTemperature(conditions.polarTemperature)
        || !validTemperature(conditions.coldTemperature)
        || !validTemperature(conditions.hotTemperature)
        || conditions.polarTemperature >= conditions.coldTemperature
        || conditions.coldTemperature >= conditions.hotTemperature) {
        throw std::invalid_argument(
            "Land type temperatures must satisfy -50 <= polar < cold < hot <= 50.");
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
    if (!validRatio(conditions.wetlandElevation)) {
        throw std::invalid_argument(
            "Wetland elevation must be between zero and one.");
    }
}

LandType classifyLandType(double normalizedElevation,
                          const climate::ClimateSample &climate,
                          const LandTypeConditions &conditions) {
    validateInputs(normalizedElevation, climate);
    validateLandTypeConditions(conditions);
    // Humidity and vegetation are independent noise fields. Treat either dry
    // signal as sufficient; requiring their rare intersection makes arid land
    // fall through to grassland or savanna instead.
    const auto isArid = climate.humidity <= conditions.dryHumidity
                        || climate.vegetation
                               <= conditions.sparseVegetation;

    if (climate.temperature <= conditions.polarTemperature)
        return LandType::Tundra;

    if (climate.temperature <= conditions.coldTemperature) {
        if (climate.humidity > conditions.dryHumidity
            && climate.vegetation >= conditions.lushVegetation) {
            return LandType::BorealForest;
        }
        return LandType::Tundra;
    }

    if (climate.temperature >= conditions.hotTemperature) {
        if (isArid) {
            return LandType::Desert;
        }
        if (climate.humidity >= conditions.wetHumidity
            && climate.vegetation >= conditions.lushVegetation) {
            return LandType::Rainforest;
        }
        if (climate.humidity > conditions.dryHumidity
            && climate.vegetation >= conditions.lushVegetation) {
            return LandType::TropicalForest;
        }
        return LandType::Savanna;
    }

    if (normalizedElevation <= conditions.wetlandElevation
        && climate.humidity >= conditions.wetHumidity
        && climate.vegetation >= conditions.lushVegetation) {
        return LandType::Wetland;
    }
    if (isArid) {
        return LandType::Steppe;
    }
    if (climate.humidity > conditions.dryHumidity
        && climate.vegetation >= conditions.lushVegetation) {
        return LandType::TemperateForest;
    }
    return LandType::Grassland;
}

} // namespace worldgen
