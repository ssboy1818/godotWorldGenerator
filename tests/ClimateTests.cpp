#include "ClimateGenerator.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

using namespace worldgen;
using namespace worldgen::climate;

constexpr double tolerance = 1e-12;

void require(bool condition, std::string_view message) {
    if (!condition)
        throw std::runtime_error{message.data()};
}

void requireNear(double actual, double expected, std::string_view message) {
    require(std::abs(actual - expected) <= tolerance, message);
}

ClimateGenerationSettings settings() {
    return {
        .bounds = {{0.0, 0.0}, {100.0, 100.0}},
        .seed = 71,
        .equatorTemperature = 30.0,
        .poleTemperature = -20.0,
        .vegetationCoefficient = 1.0,
        .humidityCoefficient = 1.0,
        .noiseOctaves = 4,
        .noiseFrequency = 0.017,
        .noiseLacunarity = 2.0,
        .noisePersistence = 0.5,
    };
}

void testLatitudeTemperature() {
    auto value = settings();
    value.temperatureNoiseStrength = 0.0;
    value.temperatureElevationCooling = 0.0;
    value.temperatureHumidityInfluence = 0.0;
    const ClimateGenerator generator{value};

    requireNear(generator.temperature({30.0, 0.0}, 0.0, 0.5),
                -20.0,
                "The top pole does not use the pole temperature.");
    requireNear(generator.temperature({30.0, 100.0}, 0.0, 0.5),
                -20.0,
                "The bottom pole does not use the pole temperature.");
    requireNear(generator.temperature({30.0, 50.0}, 0.0, 0.5),
                30.0,
                "The world center does not use the equator temperature.");
    requireNear(generator.temperature({30.0, 25.0}, 0.0, 0.5),
                5.0,
                "Temperature does not interpolate by latitude.");
    requireNear(generator.temperature({30.0, 25.0}, 0.0, 0.5),
                generator.temperature({30.0, 75.0}, 0.0, 0.5),
                "Temperature is not symmetric around the equator.");
}

void testTemperatureModifiers() {
    auto value = settings();
    value.temperatureNoiseStrength = 0.0;
    value.temperatureElevationCooling = 20.0;
    value.temperatureHumidityInfluence = 4.0;
    value.temperatureLatitudeExponent = 2.0;
    const ClimateGenerator generator{value};
    const Vector2d position{30.0, 25.0};

    requireNear(generator.temperature(position, 0.0, 0.5),
                17.5,
                "Latitude exponent did not shape base temperature.");
    requireNear(generator.temperature(position, 0.5, 0.5),
                7.5,
                "Elevation did not cool temperature.");
    requireNear(generator.temperature(position, 0.0, 0.0),
                19.5,
                "Dryness did not warm temperature.");
    requireNear(generator.temperature(position, 0.0, 1.0),
                15.5,
                "Humidity did not cool temperature.");

    auto smoothSettings = settings();
    smoothSettings.temperatureNoiseStrength = 0.0;
    const ClimateGenerator smooth{smoothSettings};
    const ClimateGenerator noisy{settings()};
    const auto sample = noisy.sample(position);
    requireNear(sample.temperature,
                noisy.temperature(position, 0.0, sample.humidity),
                "Climate sample temperature does not use final modifiers.");
    require(sample.temperature
                == noisy.temperature(position, 0.0, sample.humidity),
            "Temperature noise is not deterministic.");
    require(sample.temperature
                != smooth.temperature(position, 0.0, sample.humidity),
            "Temperature noise did not perturb the latitude gradient.");
}

void testClimateNoise() {
    const auto baseSettings = settings();
    const ClimateGenerator generator{baseSettings};
    const Vector2d position{23.5, 37.25};
    const auto sample = generator.sample(position);
    const auto repeated = generator.sample(position);

    require(sample.humidity >= 0.0 && sample.humidity <= 1.0,
            "Humidity is outside its normalized range.");
    require(sample.vegetation >= 0.0 && sample.vegetation <= 1.0,
            "Vegetation is outside its normalized range.");
    require(sample.humidity == repeated.humidity
                && sample.vegetation == repeated.vegetation,
            "Climate noise is not deterministic.");
    require(sample.humidity == generator.humidity(position)
                && sample.vegetation == generator.vegetation(position),
            "Individual climate fields disagree with the combined sample.");
    require(sample.humidity != sample.vegetation,
            "Humidity and vegetation use the same noise domain.");

    auto amplifiedSettings = baseSettings;
    amplifiedSettings.humidityCoefficient = 2.0;
    amplifiedSettings.vegetationCoefficient = 2.0;
    const auto amplified = ClimateGenerator{amplifiedSettings}.sample(position);
    requireNear(amplified.humidity,
                std::min(sample.humidity * 2.0, 1.0),
                "Humidity coefficient does not scale normalized noise.");
    requireNear(amplified.vegetation,
                std::min(sample.vegetation * 2.0, 1.0),
                "Vegetation coefficient does not scale normalized noise.");

    auto disabledSettings = baseSettings;
    disabledSettings.humidityCoefficient = 0.0;
    disabledSettings.vegetationCoefficient = 0.0;
    const auto disabled = ClimateGenerator{disabledSettings}.sample(position);
    requireNear(disabled.humidity, 0.0, "Zero did not disable humidity.");
    requireNear(disabled.vegetation, 0.0, "Zero did not disable vegetation.");

    auto otherSeedSettings = baseSettings;
    ++otherSeedSettings.seed;
    const auto otherSeed = ClimateGenerator{otherSeedSettings}.sample(position);
    require(sample.temperature != otherSeed.temperature
                || sample.humidity != otherSeed.humidity
                || sample.vegetation != otherSeed.vegetation,
            "Climate noise does not respond to the generation seed.");
}

void testValidation() {
    auto value = settings();
    value.equatorTemperature = 50.01;
    try {
        static_cast<void>(ClimateGenerator{value});
        require(false, "An equator temperature above 50 was accepted.");
    } catch (const std::invalid_argument &) {
    }

    value = settings();
    value.poleTemperature = 31.0;
    try {
        static_cast<void>(ClimateGenerator{value});
        require(false, "A pole warmer than the equator was accepted.");
    } catch (const std::invalid_argument &) {
    }

    value = settings();
    value.humidityCoefficient = -0.01;
    try {
        static_cast<void>(ClimateGenerator{value});
        require(false, "A negative humidity coefficient was accepted.");
    } catch (const std::invalid_argument &) {
    }

    value = settings();
    value.vegetationCoefficient = 2.01;
    try {
        static_cast<void>(ClimateGenerator{value});
        require(false, "A vegetation coefficient above two was accepted.");
    } catch (const std::invalid_argument &) {
    }

    value = settings();
    value.temperatureNoiseStrength = 100.01;
    try {
        static_cast<void>(ClimateGenerator{value});
        require(false, "Temperature noise strength above 100 was accepted.");
    } catch (const std::invalid_argument &) {
    }

    value = settings();
    value.temperatureNoiseFrequency = 0.0;
    try {
        static_cast<void>(ClimateGenerator{value});
        require(false, "A zero temperature noise frequency was accepted.");
    } catch (const std::invalid_argument &) {
    }

    value = settings();
    value.temperatureElevationCooling = -0.01;
    try {
        static_cast<void>(ClimateGenerator{value});
        require(false, "Negative temperature elevation cooling was accepted.");
    } catch (const std::invalid_argument &) {
    }

    value = settings();
    value.temperatureHumidityInfluence =
        std::numeric_limits<double>::infinity();
    try {
        static_cast<void>(ClimateGenerator{value});
        require(false, "Non-finite temperature humidity influence was accepted.");
    } catch (const std::invalid_argument &) {
    }

    value = settings();
    value.temperatureLatitudeExponent = 10.01;
    try {
        static_cast<void>(ClimateGenerator{value});
        require(false, "A temperature latitude exponent above ten was accepted.");
    } catch (const std::invalid_argument &) {
    }

    const ClimateGenerator generator{settings()};
    try {
        static_cast<void>(generator.sample({50.0, 101.0}));
        require(false, "A climate position outside the bounds was accepted.");
    } catch (const std::invalid_argument &) {
    }
    try {
        static_cast<void>(generator.temperature({50.0, 50.0}, 1.01, 0.5));
        require(false, "Temperature accepted elevation above one.");
    } catch (const std::invalid_argument &) {
    }
}

} // namespace

int main() {
    try {
        testLatitudeTemperature();
        testTemperatureModifiers();
        testClimateNoise();
        testValidation();
        std::cout << "Climate tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Climate test failed: " << error.what() << '\n';
        return 1;
    }
}
