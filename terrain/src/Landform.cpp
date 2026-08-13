#include "Landform.h"

#include <cmath>
#include <stdexcept>

namespace worldgen {

bool isValidLandform(Landform landform) noexcept {
    switch (landform) {
    case Landform::Plain:
    case Landform::Hill:
    case Landform::Mountain:
        return true;
    }
    return false;
}

void validateLandformConditions(const LandformConditions &conditions) {
    const auto validRatio = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 1.0;
    };
    if (!validRatio(conditions.hillElevation)
        || !validRatio(conditions.mountainElevation)
        || conditions.hillElevation >= conditions.mountainElevation) {
        throw std::invalid_argument(
            "Landform elevations must satisfy 0 <= hill < mountain <= 1.");
    }
}

Landform classifyLandform(double normalizedElevation,
                          const LandformConditions &conditions) {
    if (!std::isfinite(normalizedElevation)
        || normalizedElevation < 0.0 || normalizedElevation > 1.0) {
        throw std::invalid_argument(
            "Normalized land elevation must be between zero and one.");
    }
    validateLandformConditions(conditions);

    if (normalizedElevation >= conditions.mountainElevation)
        return Landform::Mountain;
    if (normalizedElevation >= conditions.hillElevation)
        return Landform::Hill;
    return Landform::Plain;
}

} // namespace worldgen
