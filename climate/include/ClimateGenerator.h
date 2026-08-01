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

    double temperatureNoiseStrength{8.0};
    double temperatureNoiseFrequency{0.003};
    double temperatureElevationCooling{20.0};
    double temperatureHumidityInfluence{4.0};
    double temperatureLatitudeExponent{1.0};

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
    [[nodiscard]] double humidity(Vector2d position) const;
    [[nodiscard]] double vegetation(Vector2d position) const;
    [[nodiscard]] double temperature(Vector2d position,
                                     double normalizedElevation,
                                     double humidity) const;

private:
    const ClimateGenerationSettings m_settings;
};

} // namespace worldgen::climate
