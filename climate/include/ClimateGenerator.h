#pragma once

#include "BoundingBox.h"

#include <cstdint>

namespace worldgen::climate {

struct ClimateGenerationSettings {
    BoundingBox bounds;
    std::uint64_t seed{0};

    double equatorTemperature{30.0};
    double poleTemperature{-20.0};
    double vegetationCoefficient{1.0};
    double humidityCoefficient{1.0};

    std::uint32_t noiseOctaves{5};
    double noiseFrequency{0.01};
    double noiseLacunarity{2.0};
    double noisePersistence{0.5};
};

struct ClimateSample {
    double temperature;
    double humidity;
    double vegetation;
};

class ClimateGenerator {
public:
    explicit ClimateGenerator(ClimateGenerationSettings settings);

    [[nodiscard]] const ClimateGenerationSettings &settings() const noexcept;
    [[nodiscard]] ClimateSample sample(Vector2d position) const;

private:
    const ClimateGenerationSettings m_settings;
};

} // namespace worldgen::climate
