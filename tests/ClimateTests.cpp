#include "ClimateGenerator.h"

#include <algorithm>
#include <cmath>
#include <iostream>
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
    const ClimateGenerator generator{settings()};

    requireNear(generator.sample({30.0, 0.0}).temperature,
                -20.0,
                "The top pole does not use the pole temperature.");
    requireNear(generator.sample({30.0, 100.0}).temperature,
                -20.0,
                "The bottom pole does not use the pole temperature.");
    requireNear(generator.sample({30.0, 50.0}).temperature,
                30.0,
                "The world center does not use the equator temperature.");
    requireNear(generator.sample({30.0, 25.0}).temperature,
                5.0,
                "Temperature does not interpolate by latitude.");
    requireNear(generator.sample({30.0, 25.0}).temperature,
                generator.sample({30.0, 75.0}).temperature,
                "Temperature is not symmetric around the equator.");
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
    require(sample.humidity != otherSeed.humidity
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

    const ClimateGenerator generator{settings()};
    try {
        static_cast<void>(generator.sample({50.0, 101.0}));
        require(false, "A climate position outside the bounds was accepted.");
    } catch (const std::invalid_argument &) {
    }
}

} // namespace

int main() {
    try {
        testLatitudeTemperature();
        testClimateNoise();
        testValidation();
        std::cout << "Climate tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Climate test failed: " << error.what() << '\n';
        return 1;
    }
}
