#include "ClimateGenerator.h"

#include "PerlinNoise.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace worldgen::climate {

namespace {

constexpr std::uint64_t humiditySeedDomain = 0x48a5b9c37d21e6f0ULL;
constexpr std::uint64_t vegetationSeedDomain = 0xb731d4e8a20c5f69ULL;

void validateSettings(const ClimateGenerationSettings &settings) {
    const auto validTemperature = [](double value) {
        return std::isfinite(value) && value >= -50.0 && value <= 50.0;
    };
    if (!validTemperature(settings.equatorTemperature)
        || !validTemperature(settings.poleTemperature)) {
        throw std::invalid_argument(
            "Equator and pole temperatures must be between -50 and 50.");
    }
    if (settings.poleTemperature > settings.equatorTemperature) {
        throw std::invalid_argument(
            "Pole temperature cannot exceed equator temperature.");
    }
    const auto validCoefficient = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 2.0;
    };
    if (!validCoefficient(settings.vegetationCoefficient)) {
        throw std::invalid_argument(
            "Vegetation coefficient must be between zero and two.");
    }
    if (!validCoefficient(settings.humidityCoefficient)) {
        throw std::invalid_argument(
            "Humidity coefficient must be between zero and two.");
    }
    if (settings.noiseOctaves == 0)
        throw std::invalid_argument("Climate noise needs at least one octave.");
    if (!std::isfinite(settings.noiseFrequency)
        || settings.noiseFrequency <= 0.0) {
        throw std::invalid_argument("Climate noise frequency must be positive.");
    }
    if (!std::isfinite(settings.noiseLacunarity)
        || settings.noiseLacunarity <= 0.0) {
        throw std::invalid_argument("Climate noise lacunarity must be positive.");
    }
    if (!std::isfinite(settings.noisePersistence)
        || settings.noisePersistence < 0.0) {
        throw std::invalid_argument(
            "Climate noise persistence must be non-negative.");
    }
}

[[nodiscard]] double scaledNoise(
    Vector2d position,
    const ClimateGenerationSettings &settings,
    std::uint64_t seedDomain,
    double coefficient) {
    if (coefficient == 0.0)
        return 0.0;

    const auto value = noise::fractalNoise(position,
                                           settings.seed ^ seedDomain,
                                           settings.noiseOctaves,
                                           settings.noiseFrequency,
                                           settings.noiseLacunarity,
                                           settings.noisePersistence);
    if (!std::isfinite(value)) {
        throw std::runtime_error("Climate noise produced a non-finite value.");
    }
    return std::clamp(value * coefficient, 0.0, 1.0);
}

} // namespace

ClimateGenerator::ClimateGenerator(ClimateGenerationSettings settings)
    : m_settings(std::move(settings)) {
    validateSettings(m_settings);
}

const ClimateGenerationSettings &ClimateGenerator::settings() const noexcept {
    return m_settings;
}

ClimateSample ClimateGenerator::sample(Vector2d position) const {
    if (!m_settings.bounds.contains(position)) {
        throw std::invalid_argument(
            "A climate sample position must be inside the world bounds.");
    }

    const auto halfHeight = (m_settings.bounds.max.y - m_settings.bounds.min.y)
                            * 0.5;
    const auto centerY = m_settings.bounds.min.y + halfHeight;
    const auto latitude = std::clamp(
        std::abs(position.y - centerY) / halfHeight,
        0.0,
        1.0);
    const auto temperature = m_settings.equatorTemperature
                             + (m_settings.poleTemperature
                                - m_settings.equatorTemperature)
                                   * latitude;

    return {
        .temperature = temperature,
        .humidity = scaledNoise(position,
                                m_settings,
                                humiditySeedDomain,
                                m_settings.humidityCoefficient),
        .vegetation = scaledNoise(position,
                                  m_settings,
                                  vegetationSeedDomain,
                                  m_settings.vegetationCoefficient),
    };
}

} // namespace worldgen::climate
