#pragma once

#include <cstdint>

namespace worldgen {

enum class Landform : std::uint8_t {
    Plain = 0,
    Hill = 1,
    Mountain = 2,
};

struct LandformConditions {
    double hillElevation{0.38};
    double mountainElevation{0.68};
};

[[nodiscard]] bool isValidLandform(Landform landform) noexcept;
void validateLandformConditions(const LandformConditions &conditions);

[[nodiscard]] Landform classifyLandform(
    double normalizedElevation,
    const LandformConditions &conditions = {});

} // namespace worldgen
