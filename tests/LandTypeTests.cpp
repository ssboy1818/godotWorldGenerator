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
                 double elevation,
                 double seaLevel,
                 climate::ClimateSample sample,
                 std::string_view message) {
    require(classifyLandType(elevation, seaLevel, sample) == expected,
            message);
}

void testEveryLandType() {
    constexpr double seaLevel = 0.4;

    requireType(LandType::SnowPeaks,
                0.85,
                seaLevel,
                climate(-5.0, 0.5, 0.4),
                "Cold high land was not classified as snow peaks.");
    requireType(LandType::Mountain,
                0.85,
                seaLevel,
                climate(10.0, 0.5, 0.4),
                "High land was not classified as mountain.");
    requireType(LandType::Tundra,
                0.55,
                seaLevel,
                climate(0.0, 0.5, 0.4),
                "Cold lower land was not classified as tundra.");
    requireType(LandType::Hills,
                0.67,
                seaLevel,
                climate(15.0, 0.5, 0.4),
                "Elevated land was not classified as hills.");
    requireType(LandType::Swamp,
                0.46,
                seaLevel,
                climate(18.0, 0.8, 0.7),
                "Wet vegetated lowland was not classified as swamp.");
    requireType(LandType::Beach,
                0.43,
                seaLevel,
                climate(18.0, 0.4, 0.4),
                "Shoreline land was not classified as beach.");
    requireType(LandType::Rainforest,
                0.55,
                seaLevel,
                climate(25.0, 0.8, 0.8),
                "Hot lush land was not classified as rainforest.");
    requireType(LandType::Desert,
                0.55,
                seaLevel,
                climate(25.0, 0.2, 0.2),
                "Hot dry land was not classified as desert.");
    requireType(LandType::Forest,
                0.55,
                seaLevel,
                climate(15.0, 0.6, 0.7),
                "Wooded land was not classified as forest.");
    requireType(LandType::Sparse,
                0.55,
                seaLevel,
                climate(15.0, 0.2, 0.2),
                "Thinly covered land was not classified as sparse.");
    requireType(LandType::Fields,
                0.55,
                seaLevel,
                climate(15.0, 0.5, 0.4),
                "Temperate moderate land was not classified as fields.");
}

void testNormalizedElevation() {
    const auto sample = climate(15.0, 0.5, 0.4);
    requireType(LandType::Mountain,
                0.86,
                0.5,
                sample,
                "Mountain classification ignored normalized height above sea level.");
    requireType(LandType::Beach,
                0.904,
                0.9,
                sample,
                "Beach classification ignored a high local sea level.");
}

void testValidation() {
    require(!isValidLandType(static_cast<LandType>(255)),
            "An unknown land type value was accepted.");

    try {
        static_cast<void>(classifyLandType(
            0.3,
            0.4,
            climate(15.0, 0.5, 0.5)));
        require(false, "Underwater terrain was accepted as land.");
    } catch (const std::invalid_argument &) {
    }

    try {
        static_cast<void>(classifyLandType(
            0.5,
            0.4,
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
        testEveryLandType();
        testNormalizedElevation();
        testValidation();
        std::cout << "Land type tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Land type test failed: " << error.what() << '\n';
        return 1;
    }
}
