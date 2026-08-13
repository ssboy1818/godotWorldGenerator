#include "Landform.h"
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
    require(conditions.polarTemperature == 0.0
                && conditions.coldTemperature == 6.0
                && conditions.hotTemperature == 20.0,
            "Default land temperature conditions changed unexpectedly.");
    require(conditions.dryHumidity == 0.45
                && conditions.wetHumidity == 0.62,
            "Default land humidity conditions changed unexpectedly.");
    require(conditions.sparseVegetation == 0.45
                && conditions.lushVegetation == 0.54,
            "Default land vegetation conditions changed unexpectedly.");
    require(conditions.wetlandElevation == 0.18,
            "Default wetland elevation changed unexpectedly.");

    const auto landformConditions = LandformConditions{};
    require(landformConditions.hillElevation == 0.38
                && landformConditions.mountainElevation == 0.68,
            "Default landform elevations changed unexpectedly.");
}

void testEveryLandType() {
    requireType(LandType::Tundra,
                0.3,
                climate(0.0, 0.5, 0.2),
                "Polar land was not classified as tundra.");
    requireType(LandType::BorealForest,
                0.3,
                climate(4.0, 0.6, 0.7),
                "Cool lush land was not classified as boreal forest.");
    requireType(LandType::Grassland,
                0.3,
                climate(15.0, 0.5, 0.5),
                "Temperate moderate land was not classified as grassland.");
    requireType(LandType::TemperateForest,
                0.3,
                climate(15.0, 0.6, 0.7),
                "Temperate wooded land was not classified as forest.");
    requireType(LandType::Steppe,
                0.3,
                climate(15.0, 0.2, 0.2),
                "Temperate dry land was not classified as steppe.");
    requireType(LandType::Wetland,
                0.1,
                climate(18.0, 0.8, 0.7),
                "Wet vegetated lowland was not classified as wetland.");
    requireType(LandType::Desert,
                0.3,
                climate(25.0, 0.2, 0.2),
                "Hot dry land was not classified as desert.");
    requireType(LandType::Savanna,
                0.3,
                climate(25.0, 0.5, 0.5),
                "Hot moderate land was not classified as savanna.");
    requireType(LandType::TropicalForest,
                0.3,
                climate(25.0, 0.55, 0.7),
                "Hot wooded land was not classified as tropical forest.");
    requireType(LandType::Rainforest,
                0.3,
                climate(25.0, 0.8, 0.8),
                "Hot lush land was not classified as rainforest.");
}

void testLandforms() {
    require(classifyLandform(0.1) == Landform::Plain,
            "Low relief was not classified as plain.");
    require(classifyLandform(0.38) == Landform::Hill,
            "The hill threshold was not inclusive.");
    require(classifyLandform(0.68) == Landform::Mountain,
            "The mountain threshold was not inclusive.");

    const auto sample = climate(15.0, 0.5, 0.5);
    requireType(LandType::Grassland,
                0.68,
                sample,
                "Mountain relief replaced a climate biome.");
    requireType(LandType::Wetland,
                0.18,
                climate(15.0, 0.8, 0.7),
                "Wetland classification ignored normalized elevation.");
}

void testTemperatureBands() {
    requireType(LandType::Tundra,
                0.3,
                climate(0.0, 0.8, 0.8),
                "Polar temperature did not dominate lush conditions.");
    requireType(LandType::Grassland,
                0.1,
                climate(15.0, 0.5, 0.5),
                "Low land became wetland without wet lush conditions.");
    requireType(LandType::Grassland,
                0.3,
                climate(15.0, 0.8, 0.5),
                "Humidity alone classified temperate land as forest.");
    requireType(LandType::Steppe,
                0.3,
                climate(15.0, 0.2, 0.7),
                "Dry temperate land with independent vegetation was not steppe.");
    requireType(LandType::Steppe,
                0.3,
                climate(15.0, 0.7, 0.2),
                "Sparse temperate land with independent humidity was not steppe.");
    requireType(LandType::Desert,
                0.3,
                climate(25.0, 0.2, 0.7),
                "Dry tropical land with independent vegetation was not desert.");
    requireType(LandType::Desert,
                0.3,
                climate(25.0, 0.7, 0.2),
                "Sparse tropical land with independent humidity was not desert.");
    requireType(LandType::Savanna,
                0.3,
                climate(25.0, 0.5, 0.5),
                "Moderate tropical land was not savanna.");

    auto conditions = LandTypeConditions{};
    conditions.hotTemperature = 30.0;
    require(classifyLandType(0.3,
                             climate(25.0, 0.2, 0.2),
                             conditions)
                == LandType::Steppe,
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

    auto landformConditions = LandformConditions{};
    landformConditions.hillElevation = landformConditions.mountainElevation;
    try {
        validateLandformConditions(landformConditions);
        require(false, "Overlapping landform elevations were accepted.");
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
        testLandforms();
        testTemperatureBands();
        testValidation();
        std::cout << "Land type tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Land type test failed: " << error.what() << '\n';
        return 1;
    }
}
