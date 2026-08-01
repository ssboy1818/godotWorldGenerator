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
constexpr std::uint64_t temperatureSeedDomain = 0x7c4e91a2d6b835f0ULL;

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
    const auto validTemperatureModifier = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 100.0;
    };
    if (!validTemperatureModifier(settings.temperatureNoiseStrength)) {
        throw std::invalid_argument(
            "Temperature noise strength must be between zero and 100.");
    }
    if (!std::isfinite(settings.temperatureNoiseFrequency)
        || settings.temperatureNoiseFrequency <= 0.0) {
        throw std::invalid_argument(
            "Temperature noise frequency must be positive.");
    }
    if (!validTemperatureModifier(settings.temperatureElevationCooling)) {
        throw std::invalid_argument(
            "Temperature elevation cooling must be between zero and 100.");
    }
    if (!validTemperatureModifier(settings.temperatureHumidityInfluence)) {
        throw std::invalid_argument(
            "Temperature humidity influence must be between zero and 100.");
    }
    if (!std::isfinite(settings.temperatureLatitudeExponent)
        || settings.temperatureLatitudeExponent <= 0.0
        || settings.temperatureLatitudeExponent > 10.0) {
        throw std::invalid_argument(
            "Temperature latitude exponent must be between zero and ten.");
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
    const auto humidityValue = humidity(position);
    const auto vegetationValue = vegetation(position);
    return {
        .temperature = temperature(position, 0.0, humidityValue),
        .humidity = humidityValue,
        .vegetation = vegetationValue,
    };
}

double ClimateGenerator::humidity(Vector2d position) const {
    if (!m_settings.bounds.contains(position)) {
        throw std::invalid_argument(
            "A humidity position must be inside the world bounds.");
    }
    return scaledNoise(position,
                       m_settings,
                       humiditySeedDomain,
                       m_settings.humidityCoefficient);
}

double ClimateGenerator::vegetation(Vector2d position) const {
    if (!m_settings.bounds.contains(position)) {
        throw std::invalid_argument(
            "A vegetation position must be inside the world bounds.");
    }
    return scaledNoise(position,
                       m_settings,
                       vegetationSeedDomain,
                       m_settings.vegetationCoefficient);
}

double ClimateGenerator::temperature(Vector2d position,
                                     double normalizedElevation,
                                     double humidity) const {
    if (!m_settings.bounds.contains(position)) {
        throw std::invalid_argument(
            "A temperature position must be inside the world bounds.");
    }
    if (!std::isfinite(normalizedElevation)
        || normalizedElevation < 0.0 || normalizedElevation > 1.0) {
        throw std::invalid_argument(
            "Normalized temperature elevation must be between zero and one.");
    }
    if (!std::isfinite(humidity) || humidity < 0.0 || humidity > 1.0) {
        throw std::invalid_argument(
            "Temperature humidity must be between zero and one.");
    }

    const auto halfHeight = (m_settings.bounds.max.y - m_settings.bounds.min.y)
                            * 0.5;
    const auto centerY = m_settings.bounds.min.y + halfHeight;
    const auto latitude = std::clamp(
        std::abs(position.y - centerY) / halfHeight,
        0.0,
        1.0);
    const auto latitudeAmount = std::pow(
        latitude,
        m_settings.temperatureLatitudeExponent);
    const auto baseTemperature = m_settings.equatorTemperature
                                 + (m_settings.poleTemperature
                                    - m_settings.equatorTemperature)
                                       * latitudeAmount;
    auto noiseOffset = 0.0;
    if (m_settings.temperatureNoiseStrength > 0.0) {
        const auto temperatureNoise = noise::fractalNoise(
            position,
            m_settings.seed ^ temperatureSeedDomain,
            m_settings.noiseOctaves,
            m_settings.temperatureNoiseFrequency,
            m_settings.noiseLacunarity,
            m_settings.noisePersistence);
        if (!std::isfinite(temperatureNoise)) {
            throw std::runtime_error(
                "Temperature noise produced a non-finite value.");
        }
        noiseOffset = (temperatureNoise * 2.0 - 1.0)
                      * m_settings.temperatureNoiseStrength;
    }
    const auto elevationCooling = normalizedElevation
                                  * m_settings.temperatureElevationCooling;
    const auto humidityOffset = (0.5 - humidity)
                                * m_settings.temperatureHumidityInfluence;
    return std::clamp(baseTemperature
                          + noiseOffset
                          - elevationCooling
                          + humidityOffset,
                      -50.0,
                      50.0);
}

} // namespace worldgen::climate
