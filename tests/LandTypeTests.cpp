#include "LandType.h"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

using namespace worldgen;

void require(bool condition, std::string_view message) {
    if (!condition)
        throw std::runtime_error{message.data()};
}

climate::ClimateSample climate(double temperature,
                               double humidity,
                               double vegetation) {
    return {
        .temperature = temperature,
        .humidity = humidity,
        .vegetation = vegetation,
    };
}

void requireType(LandType expected,
                 double normalizedElevation,
                 climate::ClimateSample sample,
                 std::string_view message) {
    require(classifyLandType(normalizedElevation, sample) == expected,
            message);
}

void testDefaultConditions() {
    const auto conditions = LandTypeConditions{};
    require(conditions.snowTemperature == 0.0
                && conditions.coldTemperature == 6.0
                && conditions.hotTemperature == 20.0,
            "Default land temperature conditions changed unexpectedly.");
    require(conditions.dryHumidity == 0.45
                && conditions.wetHumidity == 0.62,
            "Default land humidity conditions changed unexpectedly.");
    require(conditions.sparseVegetation == 0.45
                && conditions.lushVegetation == 0.54,
            "Default land vegetation conditions changed unexpectedly.");
    require(conditions.lowlandElevation == 0.18
                && conditions.hillElevation == 0.38
                && conditions.mountainElevation == 0.68,
            "Default land elevation conditions changed unexpectedly.");
}

void testEveryLandType() {
    requireType(LandType::SnowPeaks,
                0.8,
                climate(-5.0, 0.5, 0.4),
                "Cold high land was not classified as snow peaks.");
    requireType(LandType::Mountain,
                0.8,
                climate(10.0, 0.5, 0.4),
                "High land was not classified as mountain.");
    requireType(LandType::Tundra,
                0.3,
                climate(0.0, 0.5, 0.2),
                "Cold lower land was not classified as tundra.");
    requireType(LandType::Hills,
                0.5,
                climate(15.0, 0.5, 0.4),
                "Elevated land was not classified as hills.");
    requireType(LandType::Swamp,
                0.1,
                climate(18.0, 0.8, 0.7),
                "Wet vegetated lowland was not classified as swamp.");
    requireType(LandType::Rainforest,
                0.3,
                climate(25.0, 0.8, 0.8),
                "Hot lush land was not classified as rainforest.");
    requireType(LandType::Desert,
                0.3,
                climate(25.0, 0.2, 0.2),
                "Hot dry land was not classified as desert.");
    requireType(LandType::Forest,
                0.3,
                climate(15.0, 0.6, 0.7),
                "Wooded land was not classified as forest.");
    requireType(LandType::Sparse,
                0.3,
                climate(15.0, 0.2, 0.2),
                "Thinly covered land was not classified as sparse.");
    requireType(LandType::Fields,
                0.3,
                climate(15.0, 0.5, 0.4),
                "Temperate moderate land was not classified as fields.");
}

void testElevationThresholds() {
    const auto sample = climate(15.0, 0.5, 0.4);
    requireType(LandType::Mountain,
                0.68,
                sample,
                "Mountain classification ignored normalized elevation.");
    requireType(LandType::Swamp,
                0.18,
                climate(15.0, 0.8, 0.7),
                "Lowland classification ignored normalized elevation.");
}

void testClimateSynergy() {
    requireType(LandType::Fields,
                0.3,
                climate(0.0, 0.8, 0.8),
                "Cold lush land became tundra from temperature alone.");
    requireType(LandType::Fields,
                0.1,
                climate(15.0, 0.5, 0.5),
                "Low land became swamp without wet lush conditions.");
    requireType(LandType::Fields,
                0.3,
                climate(15.0, 0.8, 0.5),
                "Humidity alone classified land as forest.");

    auto conditions = LandTypeConditions{};
    conditions.hotTemperature = 30.0;
    require(classifyLandType(0.3,
                             climate(25.0, 0.2, 0.2),
                             conditions)
                == LandType::Sparse,
            "Custom hot-temperature conditions did not affect classification.");
}

void testValidation() {
    require(!isValidLandType(static_cast<LandType>(255)),
            "An unknown land type value was accepted.");

    auto conditions = LandTypeConditions{};
    conditions.dryHumidity = conditions.wetHumidity;
    try {
        validateLandTypeConditions(conditions);
        require(false, "Overlapping humidity conditions were accepted.");
    } catch (const std::invalid_argument &) {
    }

    try {
        static_cast<void>(classifyLandType(
            -0.01,
            climate(15.0, 0.5, 0.5)));
        require(false, "Negative normalized elevation was accepted.");
    } catch (const std::invalid_argument &) {
    }

    try {
        static_cast<void>(classifyLandType(
            0.5,
            climate(15.0,
                    std::numeric_limits<double>::quiet_NaN(),
                    0.5)));
        require(false, "A non-finite land climate was accepted.");
    } catch (const std::invalid_argument &) {
    }
}

} // namespace

int main() {
    try {
        testDefaultConditions();
        testEveryLandType();
        testElevationThresholds();
        testClimateSynergy();
        testValidation();
        std::cout << "Land type tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Land type test failed: " << error.what() << '\n';
        return 1;
    }
}
