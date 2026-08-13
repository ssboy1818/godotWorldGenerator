#include "VoronoiWorldGenerator.h"

#include "WorldGenerator.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <stdexcept>

namespace worldgen {

namespace {

[[nodiscard]] WorldGenerationSettings toCoreSettings(
    const WorldgenSettings &settings) {
    const auto columns = settings.columns();
    const auto rows = settings.rows();
    if (columns <= 0 || rows <= 0)
        throw std::invalid_argument("Site columns and rows must be positive.");

    if (static_cast<std::uint64_t>(columns)
            > std::numeric_limits<std::size_t>::max()
        || static_cast<std::uint64_t>(rows)
               > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("Site grid dimensions exceed the native size range.");
    }
    constexpr auto maximumCellCount = static_cast<std::uint64_t>(
        std::numeric_limits<std::int32_t>::max() - 1);
    if (static_cast<std::uint64_t>(columns)
        > maximumCellCount / static_cast<std::uint64_t>(rows)) {
        throw std::invalid_argument(
            "The site grid has too many cells for PackedInt32Array offsets.");
    }

    const auto noiseOctaves = settings.noiseOctaves();
    if (noiseOctaves <= 0
        || static_cast<std::uint64_t>(noiseOctaves)
               > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("Noise octaves must fit in a positive 32-bit integer.");
    }

    const auto riverSourceCount = settings.riverSourceCount();
    if (riverSourceCount < 0
        || static_cast<std::uint64_t>(riverSourceCount)
               > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("River source count exceeds the native size range.");
    }

    const auto provinceMinimumRegionCount =
        settings.provinceMinimumRegionCount();
    if (provinceMinimumRegionCount <= 0
        || static_cast<std::uint64_t>(provinceMinimumRegionCount)
               > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument(
            "Minimum province region count must fit in a positive native size.");
    }
    const auto provinceMaximumRegionCount =
        settings.provinceMaximumRegionCount();
    if (provinceMaximumRegionCount < 0
        || static_cast<std::uint64_t>(provinceMaximumRegionCount)
               > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument(
            "Maximum province region count must fit in a non-negative native size.");
    }
    if (provinceMaximumRegionCount != 0
        && provinceMaximumRegionCount < provinceMinimumRegionCount) {
        throw std::invalid_argument(
            "Maximum province region count must be zero or at least the minimum.");
    }

    const auto bounds = settings.bounds();
    const Vector2d minimum{
        static_cast<double>(bounds.position.x),
        static_cast<double>(bounds.position.y),
    };
    const Vector2d maximum{
        minimum.x + static_cast<double>(bounds.size.x),
        minimum.y + static_cast<double>(bounds.size.y),
    };
    const auto edgeDecayRatio = settings.edgeDecayRatio();

    return {
        .bounds = {minimum, maximum},
        .seed = static_cast<std::uint64_t>(settings.seed()),
        .columns = static_cast<std::size_t>(columns),
        .rows = static_cast<std::size_t>(rows),
        .jitter = settings.jitter(),
        .seaLevel = settings.seaLevel(),
        .edgeDecayRatio = {
            static_cast<double>(edgeDecayRatio.x),
            static_cast<double>(edgeDecayRatio.y),
        },
        .edgeStrength = settings.edgeStrength(),
        .noiseOctaves = static_cast<std::uint32_t>(noiseOctaves),
        .noiseFrequency = settings.noiseFrequency(),
        .noiseLacunarity = settings.noiseLacunarity(),
        .noisePersistence = settings.noisePersistence(),
        .equatorTemperature = settings.equatorTemperature(),
        .poleTemperature = settings.poleTemperature(),
        .vegetationCoefficient = settings.vegetationCoefficient(),
        .humidityCoefficient = settings.humidityCoefficient(),
        .temperatureNoiseStrength = settings.temperatureNoiseStrength(),
        .temperatureNoiseFrequency = settings.temperatureNoiseFrequency(),
        .temperatureElevationCooling = settings.temperatureElevationCooling(),
        .temperatureHumidityInfluence = settings.temperatureHumidityInfluence(),
        .temperatureLatitudeExponent = settings.temperatureLatitudeExponent(),
        .oceanHumidityCoefficient = settings.oceanHumidityCoefficient(),
        .oceanHumidityDistanceRatio = settings.oceanHumidityDistanceRatio(),
        .landTypeConditions = {
            .polarTemperature = settings.landTypePolarTemperature(),
            .coldTemperature = settings.landTypeColdTemperature(),
            .hotTemperature = settings.landTypeHotTemperature(),
            .dryHumidity = settings.landTypeDryHumidity(),
            .wetHumidity = settings.landTypeWetHumidity(),
            .sparseVegetation = settings.landTypeSparseVegetation(),
            .lushVegetation = settings.landTypeLushVegetation(),
            .wetlandElevation = settings.landTypeWetlandElevation(),
        },
        .landformConditions = {
            .hillElevation = settings.landformHillElevation(),
            .mountainElevation = settings.landformMountainElevation(),
        },
        .riverSourceCount = static_cast<std::size_t>(riverSourceCount),
        .riverMinimumSourceElevation = settings.riverMinimumSourceElevation(),
        .riverRandomness = settings.riverRandomness(),
        .riverElevationTolerance = settings.riverElevationTolerance(),
        .riverHumidityCoefficient = settings.riverHumidityCoefficient(),
        .riverVegetationCoefficient = settings.riverVegetationCoefficient(),
        .provinceStartScore = settings.provinceStartScore(),
        .provinceRiverContribution = settings.provinceRiverContribution(),
        .provinceElevationContribution = settings.provinceElevationContribution(),
        .provinceDistanceContribution = settings.provinceDistanceContribution(),
        .provinceLandTypeContribution =
            settings.provinceLandTypeContribution(),
        .provinceShortBorderContribution =
            settings.provinceShortBorderContribution(),
        .provinceBaseCost = settings.provinceBaseCost(),
        .provinceSeedMinimumDistance =
            settings.provinceSeedMinimumDistance(),
        .provinceMinimumRegionCount =
            static_cast<std::size_t>(provinceMinimumRegionCount),
        .provinceMaximumRegionCount =
            static_cast<std::size_t>(provinceMaximumRegionCount),
    };
}

} // namespace

void VoronoiWorldGenerator::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("set_settings", "settings"),
                                &VoronoiWorldGenerator::setSettings);
    godot::ClassDB::bind_method(godot::D_METHOD("get_settings"),
                                &VoronoiWorldGenerator::settings);
    godot::ClassDB::bind_method(godot::D_METHOD("generate"),
                                &VoronoiWorldGenerator::generate);
    godot::ClassDB::bind_method(godot::D_METHOD("generate_async"),
                                &VoronoiWorldGenerator::generateAsync);

    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT,
                                     "settings",
                                     godot::PROPERTY_HINT_RESOURCE_TYPE,
                                     "WorldgenSettings",
                                     godot::PROPERTY_USAGE_DEFAULT
                                         | godot::PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT),
                 "set_settings", "get_settings");
}

VoronoiWorldGenerator::VoronoiWorldGenerator() = default;

void VoronoiWorldGenerator::setSettings(
    const godot::Ref<WorldgenSettings> &settings) {
    m_settings = settings;
}

godot::Ref<WorldgenSettings> VoronoiWorldGenerator::settings() const {
    return m_settings;
}

godot::Ref<VoronoiWorldData> VoronoiWorldGenerator::generate() {
    if (m_settings.is_null()) {
        godot::UtilityFunctions::push_error(
            "VoronoiWorldGenerator requires a WorldgenSettings resource.");
        return {};
    }

    try {
        const WorldGenerator generator{toCoreSettings(*m_settings.ptr())};
        const auto world = generator.generate();

        godot::Ref<VoronoiWorldData> result;
        result.instantiate();
        result->populate(world);
        return result;
    } catch (const std::exception &error) {
        godot::UtilityFunctions::push_error(
            "World generation failed: ", godot::String{error.what()});
        return {};
    }
}

godot::Ref<VoronoiWorldGenerationTask>
VoronoiWorldGenerator::generateAsync() {
    godot::Ref<VoronoiWorldGenerationTask> task;
    task.instantiate();

    if (m_settings.is_null()) {
        task->failDeferred(
            "VoronoiWorldGenerator requires a WorldgenSettings resource.");
        return task;
    }

    try {
        task->start(toCoreSettings(*m_settings.ptr()));
    } catch (const std::exception &error) {
        task->failDeferred(godot::String{error.what()});
    }
    return task;
}

} // namespace worldgen
