#include "WorldgenSettings.h"

#include <godot_cpp/core/class_db.hpp>

namespace worldgen {

void WorldgenSettings::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("set_bounds", "bounds"),
                                &WorldgenSettings::setBounds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_bounds"),
                                &WorldgenSettings::bounds);
    godot::ClassDB::bind_method(godot::D_METHOD("set_seed", "seed"),
                                &WorldgenSettings::setSeed);
    godot::ClassDB::bind_method(godot::D_METHOD("get_seed"),
                                &WorldgenSettings::seed);
    godot::ClassDB::bind_method(godot::D_METHOD("set_columns", "columns"),
                                &WorldgenSettings::setColumns);
    godot::ClassDB::bind_method(godot::D_METHOD("get_columns"),
                                &WorldgenSettings::columns);
    godot::ClassDB::bind_method(godot::D_METHOD("set_rows", "rows"),
                                &WorldgenSettings::setRows);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rows"),
                                &WorldgenSettings::rows);
    godot::ClassDB::bind_method(godot::D_METHOD("set_jitter", "jitter"),
                                &WorldgenSettings::setJitter);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jitter"),
                                &WorldgenSettings::jitter);
    godot::ClassDB::bind_method(godot::D_METHOD("set_sea_level", "sea_level"),
                                &WorldgenSettings::setSeaLevel);
    godot::ClassDB::bind_method(godot::D_METHOD("get_sea_level"),
                                &WorldgenSettings::seaLevel);
    godot::ClassDB::bind_method(godot::D_METHOD("set_edge_decay_ratio", "ratio"),
                                &WorldgenSettings::setEdgeDecayRatio);
    godot::ClassDB::bind_method(godot::D_METHOD("get_edge_decay_ratio"),
                                &WorldgenSettings::edgeDecayRatio);
    godot::ClassDB::bind_method(godot::D_METHOD("set_edge_strength", "strength"),
                                &WorldgenSettings::setEdgeStrength);
    godot::ClassDB::bind_method(godot::D_METHOD("get_edge_strength"),
                                &WorldgenSettings::edgeStrength);
    godot::ClassDB::bind_method(godot::D_METHOD("set_noise_octaves", "octaves"),
                                &WorldgenSettings::setNoiseOctaves);
    godot::ClassDB::bind_method(godot::D_METHOD("get_noise_octaves"),
                                &WorldgenSettings::noiseOctaves);
    godot::ClassDB::bind_method(godot::D_METHOD("set_noise_frequency", "frequency"),
                                &WorldgenSettings::setNoiseFrequency);
    godot::ClassDB::bind_method(godot::D_METHOD("get_noise_frequency"),
                                &WorldgenSettings::noiseFrequency);
    godot::ClassDB::bind_method(godot::D_METHOD("set_noise_lacunarity", "lacunarity"),
                                &WorldgenSettings::setNoiseLacunarity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_noise_lacunarity"),
                                &WorldgenSettings::noiseLacunarity);
    godot::ClassDB::bind_method(godot::D_METHOD("set_noise_persistence", "persistence"),
                                &WorldgenSettings::setNoisePersistence);
    godot::ClassDB::bind_method(godot::D_METHOD("get_noise_persistence"),
                                &WorldgenSettings::noisePersistence);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_equator_temperature", "temperature"),
        &WorldgenSettings::setEquatorTemperature);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_equator_temperature"),
        &WorldgenSettings::equatorTemperature);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_pole_temperature", "temperature"),
        &WorldgenSettings::setPoleTemperature);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_pole_temperature"),
        &WorldgenSettings::poleTemperature);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_vegetation_coefficient", "coefficient"),
        &WorldgenSettings::setVegetationCoefficient);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_vegetation_coefficient"),
        &WorldgenSettings::vegetationCoefficient);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_humidity_coefficient", "coefficient"),
        &WorldgenSettings::setHumidityCoefficient);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_humidity_coefficient"),
        &WorldgenSettings::humidityCoefficient);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_temperature_noise_strength", "strength"),
        &WorldgenSettings::setTemperatureNoiseStrength);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_temperature_noise_strength"),
        &WorldgenSettings::temperatureNoiseStrength);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_temperature_noise_frequency", "frequency"),
        &WorldgenSettings::setTemperatureNoiseFrequency);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_temperature_noise_frequency"),
        &WorldgenSettings::temperatureNoiseFrequency);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_temperature_elevation_cooling", "cooling"),
        &WorldgenSettings::setTemperatureElevationCooling);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_temperature_elevation_cooling"),
        &WorldgenSettings::temperatureElevationCooling);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_temperature_humidity_influence", "influence"),
        &WorldgenSettings::setTemperatureHumidityInfluence);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_temperature_humidity_influence"),
        &WorldgenSettings::temperatureHumidityInfluence);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_temperature_latitude_exponent", "exponent"),
        &WorldgenSettings::setTemperatureLatitudeExponent);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_temperature_latitude_exponent"),
        &WorldgenSettings::temperatureLatitudeExponent);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_ocean_humidity_coefficient", "coefficient"),
        &WorldgenSettings::setOceanHumidityCoefficient);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_ocean_humidity_coefficient"),
        &WorldgenSettings::oceanHumidityCoefficient);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_ocean_humidity_distance_ratio", "ratio"),
        &WorldgenSettings::setOceanHumidityDistanceRatio);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_ocean_humidity_distance_ratio"),
        &WorldgenSettings::oceanHumidityDistanceRatio);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_land_type_polar_temperature", "temperature"),
        &WorldgenSettings::setLandTypePolarTemperature);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_land_type_polar_temperature"),
        &WorldgenSettings::landTypePolarTemperature);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_land_type_cold_temperature", "temperature"),
        &WorldgenSettings::setLandTypeColdTemperature);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_land_type_cold_temperature"),
        &WorldgenSettings::landTypeColdTemperature);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_land_type_hot_temperature", "temperature"),
        &WorldgenSettings::setLandTypeHotTemperature);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_land_type_hot_temperature"),
        &WorldgenSettings::landTypeHotTemperature);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_land_type_dry_humidity", "humidity"),
        &WorldgenSettings::setLandTypeDryHumidity);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_land_type_dry_humidity"),
        &WorldgenSettings::landTypeDryHumidity);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_land_type_wet_humidity", "humidity"),
        &WorldgenSettings::setLandTypeWetHumidity);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_land_type_wet_humidity"),
        &WorldgenSettings::landTypeWetHumidity);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_land_type_sparse_vegetation", "vegetation"),
        &WorldgenSettings::setLandTypeSparseVegetation);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_land_type_sparse_vegetation"),
        &WorldgenSettings::landTypeSparseVegetation);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_land_type_lush_vegetation", "vegetation"),
        &WorldgenSettings::setLandTypeLushVegetation);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_land_type_lush_vegetation"),
        &WorldgenSettings::landTypeLushVegetation);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_land_type_wetland_elevation", "elevation"),
        &WorldgenSettings::setLandTypeWetlandElevation);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_land_type_wetland_elevation"),
        &WorldgenSettings::landTypeWetlandElevation);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_landform_hill_elevation", "elevation"),
        &WorldgenSettings::setLandformHillElevation);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_landform_hill_elevation"),
        &WorldgenSettings::landformHillElevation);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_landform_mountain_elevation", "elevation"),
        &WorldgenSettings::setLandformMountainElevation);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_landform_mountain_elevation"),
        &WorldgenSettings::landformMountainElevation);
    godot::ClassDB::bind_method(godot::D_METHOD("set_river_source_count", "count"),
                                &WorldgenSettings::setRiverSourceCount);
    godot::ClassDB::bind_method(godot::D_METHOD("get_river_source_count"),
                                &WorldgenSettings::riverSourceCount);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_river_minimum_source_elevation", "elevation"),
        &WorldgenSettings::setRiverMinimumSourceElevation);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_river_minimum_source_elevation"),
        &WorldgenSettings::riverMinimumSourceElevation);
    godot::ClassDB::bind_method(godot::D_METHOD("set_river_randomness", "randomness"),
                                &WorldgenSettings::setRiverRandomness);
    godot::ClassDB::bind_method(godot::D_METHOD("get_river_randomness"),
                                &WorldgenSettings::riverRandomness);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_river_elevation_tolerance", "tolerance"),
        &WorldgenSettings::setRiverElevationTolerance);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_river_elevation_tolerance"),
        &WorldgenSettings::riverElevationTolerance);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_river_humidity_coefficient", "coefficient"),
        &WorldgenSettings::setRiverHumidityCoefficient);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_river_humidity_coefficient"),
        &WorldgenSettings::riverHumidityCoefficient);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_river_vegetation_coefficient", "coefficient"),
        &WorldgenSettings::setRiverVegetationCoefficient);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_river_vegetation_coefficient"),
        &WorldgenSettings::riverVegetationCoefficient);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_province_start_score", "score"),
        &WorldgenSettings::setProvinceStartScore);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_province_start_score"),
        &WorldgenSettings::provinceStartScore);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_province_river_contribution", "contribution"),
        &WorldgenSettings::setProvinceRiverContribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_province_river_contribution"),
        &WorldgenSettings::provinceRiverContribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_province_elevation_contribution", "contribution"),
        &WorldgenSettings::setProvinceElevationContribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_province_elevation_contribution"),
        &WorldgenSettings::provinceElevationContribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_province_distance_contribution", "contribution"),
        &WorldgenSettings::setProvinceDistanceContribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_province_distance_contribution"),
        &WorldgenSettings::provinceDistanceContribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_province_land_type_contribution", "contribution"),
        &WorldgenSettings::setProvinceLandTypeContribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_province_land_type_contribution"),
        &WorldgenSettings::provinceLandTypeContribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_province_short_border_contribution", "contribution"),
        &WorldgenSettings::setProvinceShortBorderContribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_province_short_border_contribution"),
        &WorldgenSettings::provinceShortBorderContribution);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_province_base_cost", "cost"),
        &WorldgenSettings::setProvinceBaseCost);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_province_base_cost"),
        &WorldgenSettings::provinceBaseCost);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_province_minimum_region_count", "count"),
        &WorldgenSettings::setProvinceMinimumRegionCount);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_province_minimum_region_count"),
        &WorldgenSettings::provinceMinimumRegionCount);

    ADD_GROUP("World", "");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::RECT2, "bounds"),
                 "set_bounds", "get_bounds");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "seed"),
                 "set_seed", "get_seed");

    ADD_GROUP("Sites", "");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT,
                                     "columns",
                                     godot::PROPERTY_HINT_RANGE,
                                     "1,4096,1,or_greater"),
                 "set_columns", "get_columns");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT,
                                     "rows",
                                     godot::PROPERTY_HINT_RANGE,
                                     "1,4096,1,or_greater"),
                 "set_rows", "get_rows");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "jitter",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.01"),
                 "set_jitter", "get_jitter");

    ADD_GROUP("Terrain", "");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "sea_level",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_sea_level", "get_sea_level");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::VECTOR2, "edge_decay_ratio"),
                 "set_edge_decay_ratio", "get_edge_decay_ratio");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "edge_strength",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_edge_strength", "get_edge_strength");

    ADD_GROUP("Noise", "");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT,
                                     "noise_octaves",
                                     godot::PROPERTY_HINT_RANGE,
                                     "1,32,1,or_greater"),
                 "set_noise_octaves", "get_noise_octaves");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "noise_frequency",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0.000001,1,0.000001,or_greater"),
                 "set_noise_frequency", "get_noise_frequency");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "noise_lacunarity",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0.000001,8,0.001,or_greater"),
                 "set_noise_lacunarity", "get_noise_lacunarity");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "noise_persistence",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001,or_greater"),
                 "set_noise_persistence", "get_noise_persistence");

    ADD_GROUP("Climate", "");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "equator_temperature",
                                     godot::PROPERTY_HINT_RANGE,
                                     "-50,50,0.1"),
                 "set_equator_temperature", "get_equator_temperature");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "pole_temperature",
                                     godot::PROPERTY_HINT_RANGE,
                                     "-50,50,0.1"),
                 "set_pole_temperature", "get_pole_temperature");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "vegetation_coefficient",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,2,0.01"),
                 "set_vegetation_coefficient",
                 "get_vegetation_coefficient");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "humidity_coefficient",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,2,0.01"),
                 "set_humidity_coefficient", "get_humidity_coefficient");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "temperature_noise_strength",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,100,0.1"),
                 "set_temperature_noise_strength",
                 "get_temperature_noise_strength");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "temperature_noise_frequency",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0.000001,1,0.000001,or_greater"),
                 "set_temperature_noise_frequency",
                 "get_temperature_noise_frequency");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "temperature_elevation_cooling",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,100,0.1"),
                 "set_temperature_elevation_cooling",
                 "get_temperature_elevation_cooling");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "temperature_humidity_influence",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,100,0.1"),
                 "set_temperature_humidity_influence",
                 "get_temperature_humidity_influence");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "temperature_latitude_exponent",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0.01,10,0.01"),
                 "set_temperature_latitude_exponent",
                 "get_temperature_latitude_exponent");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "ocean_humidity_coefficient",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_ocean_humidity_coefficient",
                 "get_ocean_humidity_coefficient");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "ocean_humidity_distance_ratio",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_ocean_humidity_distance_ratio",
                 "get_ocean_humidity_distance_ratio");

    ADD_GROUP("Land Types", "land_type_");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "land_type_polar_temperature",
                                     godot::PROPERTY_HINT_RANGE,
                                     "-50,50,0.1"),
                 "set_land_type_polar_temperature",
                 "get_land_type_polar_temperature");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "land_type_cold_temperature",
                                     godot::PROPERTY_HINT_RANGE,
                                     "-50,50,0.1"),
                 "set_land_type_cold_temperature",
                 "get_land_type_cold_temperature");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "land_type_hot_temperature",
                                     godot::PROPERTY_HINT_RANGE,
                                     "-50,50,0.1"),
                 "set_land_type_hot_temperature",
                 "get_land_type_hot_temperature");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "land_type_dry_humidity",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_land_type_dry_humidity",
                 "get_land_type_dry_humidity");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "land_type_wet_humidity",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_land_type_wet_humidity",
                 "get_land_type_wet_humidity");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "land_type_sparse_vegetation",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_land_type_sparse_vegetation",
                 "get_land_type_sparse_vegetation");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "land_type_lush_vegetation",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_land_type_lush_vegetation",
                 "get_land_type_lush_vegetation");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "land_type_wetland_elevation",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_land_type_wetland_elevation",
                 "get_land_type_wetland_elevation");

    ADD_GROUP("Landforms", "landform_");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "landform_hill_elevation",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_landform_hill_elevation",
                 "get_landform_hill_elevation");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "landform_mountain_elevation",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_landform_mountain_elevation",
                 "get_landform_mountain_elevation");

    ADD_GROUP("Rivers", "");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT,
                                     "river_source_count",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1024,1,or_greater"),
                 "set_river_source_count", "get_river_source_count");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "river_minimum_source_elevation",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_river_minimum_source_elevation",
                 "get_river_minimum_source_elevation");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "river_randomness",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_river_randomness", "get_river_randomness");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "river_elevation_tolerance",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,0.25,0.001,or_greater"),
                 "set_river_elevation_tolerance",
                 "get_river_elevation_tolerance");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "river_humidity_coefficient",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_river_humidity_coefficient",
                 "get_river_humidity_coefficient");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "river_vegetation_coefficient",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1,0.001"),
                 "set_river_vegetation_coefficient",
                 "get_river_vegetation_coefficient");

    ADD_GROUP("Provinces", "");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "province_start_score",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1000,0.01,or_greater"),
                 "set_province_start_score", "get_province_start_score");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "province_river_contribution",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1000,0.01,or_greater"),
                 "set_province_river_contribution",
                 "get_province_river_contribution");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "province_elevation_contribution",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1000,0.01,or_greater"),
                 "set_province_elevation_contribution",
                 "get_province_elevation_contribution");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "province_distance_contribution",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1000,0.01,or_greater"),
                 "set_province_distance_contribution",
                 "get_province_distance_contribution");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "province_land_type_contribution",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1000,0.01,or_greater"),
                 "set_province_land_type_contribution",
                 "get_province_land_type_contribution");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "province_short_border_contribution",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1000,0.01,or_greater"),
                 "set_province_short_border_contribution",
                 "get_province_short_border_contribution");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT,
                                     "province_base_cost",
                                     godot::PROPERTY_HINT_RANGE,
                                     "0,1000,0.01,or_greater"),
                 "set_province_base_cost", "get_province_base_cost");
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT,
                                     "province_minimum_region_count",
                                     godot::PROPERTY_HINT_RANGE,
                                     "1,1024,1,or_greater"),
                 "set_province_minimum_region_count",
                 "get_province_minimum_region_count");
}

void WorldgenSettings::setBounds(const godot::Rect2 &bounds) {
    m_bounds = bounds;
}

godot::Rect2 WorldgenSettings::bounds() const {
    return m_bounds;
}

void WorldgenSettings::setSeed(std::int64_t seed) {
    m_seed = seed;
}

std::int64_t WorldgenSettings::seed() const noexcept {
    return m_seed;
}

void WorldgenSettings::setColumns(std::int64_t columns) {
    m_columns = columns;
}

std::int64_t WorldgenSettings::columns() const noexcept {
    return m_columns;
}

void WorldgenSettings::setRows(std::int64_t rows) {
    m_rows = rows;
}

std::int64_t WorldgenSettings::rows() const noexcept {
    return m_rows;
}

void WorldgenSettings::setJitter(double jitter) {
    m_jitter = jitter;
}

double WorldgenSettings::jitter() const noexcept {
    return m_jitter;
}

void WorldgenSettings::setSeaLevel(double seaLevel) {
    m_seaLevel = seaLevel;
}

double WorldgenSettings::seaLevel() const noexcept {
    return m_seaLevel;
}

void WorldgenSettings::setEdgeDecayRatio(const godot::Vector2 &ratio) {
    m_edgeDecayRatio = ratio;
}

godot::Vector2 WorldgenSettings::edgeDecayRatio() const {
    return m_edgeDecayRatio;
}

void WorldgenSettings::setEdgeStrength(double strength) {
    m_edgeStrength = strength;
}

double WorldgenSettings::edgeStrength() const noexcept {
    return m_edgeStrength;
}

void WorldgenSettings::setNoiseOctaves(std::int64_t octaves) {
    m_noiseOctaves = octaves;
}

std::int64_t WorldgenSettings::noiseOctaves() const noexcept {
    return m_noiseOctaves;
}

void WorldgenSettings::setNoiseFrequency(double frequency) {
    m_noiseFrequency = frequency;
}

double WorldgenSettings::noiseFrequency() const noexcept {
    return m_noiseFrequency;
}

void WorldgenSettings::setNoiseLacunarity(double lacunarity) {
    m_noiseLacunarity = lacunarity;
}

double WorldgenSettings::noiseLacunarity() const noexcept {
    return m_noiseLacunarity;
}

void WorldgenSettings::setNoisePersistence(double persistence) {
    m_noisePersistence = persistence;
}

double WorldgenSettings::noisePersistence() const noexcept {
    return m_noisePersistence;
}

void WorldgenSettings::setEquatorTemperature(double temperature) {
    m_equatorTemperature = temperature;
}

double WorldgenSettings::equatorTemperature() const noexcept {
    return m_equatorTemperature;
}

void WorldgenSettings::setPoleTemperature(double temperature) {
    m_poleTemperature = temperature;
}

double WorldgenSettings::poleTemperature() const noexcept {
    return m_poleTemperature;
}

void WorldgenSettings::setVegetationCoefficient(double coefficient) {
    m_vegetationCoefficient = coefficient;
}

double WorldgenSettings::vegetationCoefficient() const noexcept {
    return m_vegetationCoefficient;
}

void WorldgenSettings::setHumidityCoefficient(double coefficient) {
    m_humidityCoefficient = coefficient;
}

double WorldgenSettings::humidityCoefficient() const noexcept {
    return m_humidityCoefficient;
}

void WorldgenSettings::setTemperatureNoiseStrength(double strength) {
    m_temperatureNoiseStrength = strength;
}

double WorldgenSettings::temperatureNoiseStrength() const noexcept {
    return m_temperatureNoiseStrength;
}

void WorldgenSettings::setTemperatureNoiseFrequency(double frequency) {
    m_temperatureNoiseFrequency = frequency;
}

double WorldgenSettings::temperatureNoiseFrequency() const noexcept {
    return m_temperatureNoiseFrequency;
}

void WorldgenSettings::setTemperatureElevationCooling(double cooling) {
    m_temperatureElevationCooling = cooling;
}

double WorldgenSettings::temperatureElevationCooling() const noexcept {
    return m_temperatureElevationCooling;
}

void WorldgenSettings::setTemperatureHumidityInfluence(double influence) {
    m_temperatureHumidityInfluence = influence;
}

double WorldgenSettings::temperatureHumidityInfluence() const noexcept {
    return m_temperatureHumidityInfluence;
}

void WorldgenSettings::setTemperatureLatitudeExponent(double exponent) {
    m_temperatureLatitudeExponent = exponent;
}

double WorldgenSettings::temperatureLatitudeExponent() const noexcept {
    return m_temperatureLatitudeExponent;
}

void WorldgenSettings::setOceanHumidityCoefficient(double coefficient) {
    m_oceanHumidityCoefficient = coefficient;
}

double WorldgenSettings::oceanHumidityCoefficient() const noexcept {
    return m_oceanHumidityCoefficient;
}

void WorldgenSettings::setOceanHumidityDistanceRatio(double ratio) {
    m_oceanHumidityDistanceRatio = ratio;
}

double WorldgenSettings::oceanHumidityDistanceRatio() const noexcept {
    return m_oceanHumidityDistanceRatio;
}

void WorldgenSettings::setLandTypePolarTemperature(double temperature) {
    m_landTypePolarTemperature = temperature;
}

double WorldgenSettings::landTypePolarTemperature() const noexcept {
    return m_landTypePolarTemperature;
}

void WorldgenSettings::setLandTypeColdTemperature(double temperature) {
    m_landTypeColdTemperature = temperature;
}

double WorldgenSettings::landTypeColdTemperature() const noexcept {
    return m_landTypeColdTemperature;
}

void WorldgenSettings::setLandTypeHotTemperature(double temperature) {
    m_landTypeHotTemperature = temperature;
}

double WorldgenSettings::landTypeHotTemperature() const noexcept {
    return m_landTypeHotTemperature;
}

void WorldgenSettings::setLandTypeDryHumidity(double humidity) {
    m_landTypeDryHumidity = humidity;
}

double WorldgenSettings::landTypeDryHumidity() const noexcept {
    return m_landTypeDryHumidity;
}

void WorldgenSettings::setLandTypeWetHumidity(double humidity) {
    m_landTypeWetHumidity = humidity;
}

double WorldgenSettings::landTypeWetHumidity() const noexcept {
    return m_landTypeWetHumidity;
}

void WorldgenSettings::setLandTypeSparseVegetation(double vegetation) {
    m_landTypeSparseVegetation = vegetation;
}

double WorldgenSettings::landTypeSparseVegetation() const noexcept {
    return m_landTypeSparseVegetation;
}

void WorldgenSettings::setLandTypeLushVegetation(double vegetation) {
    m_landTypeLushVegetation = vegetation;
}

double WorldgenSettings::landTypeLushVegetation() const noexcept {
    return m_landTypeLushVegetation;
}

void WorldgenSettings::setLandTypeWetlandElevation(double elevation) {
    m_landTypeWetlandElevation = elevation;
}

double WorldgenSettings::landTypeWetlandElevation() const noexcept {
    return m_landTypeWetlandElevation;
}

void WorldgenSettings::setLandformHillElevation(double elevation) {
    m_landformHillElevation = elevation;
}

double WorldgenSettings::landformHillElevation() const noexcept {
    return m_landformHillElevation;
}

void WorldgenSettings::setLandformMountainElevation(double elevation) {
    m_landformMountainElevation = elevation;
}

double WorldgenSettings::landformMountainElevation() const noexcept {
    return m_landformMountainElevation;
}

void WorldgenSettings::setRiverSourceCount(std::int64_t count) {
    m_riverSourceCount = count;
}

std::int64_t WorldgenSettings::riverSourceCount() const noexcept {
    return m_riverSourceCount;
}

void WorldgenSettings::setRiverMinimumSourceElevation(double elevation) {
    m_riverMinimumSourceElevation = elevation;
}

double WorldgenSettings::riverMinimumSourceElevation() const noexcept {
    return m_riverMinimumSourceElevation;
}

void WorldgenSettings::setRiverRandomness(double randomness) {
    m_riverRandomness = randomness;
}

double WorldgenSettings::riverRandomness() const noexcept {
    return m_riverRandomness;
}

void WorldgenSettings::setRiverElevationTolerance(double tolerance) {
    m_riverElevationTolerance = tolerance;
}

double WorldgenSettings::riverElevationTolerance() const noexcept {
    return m_riverElevationTolerance;
}

void WorldgenSettings::setRiverHumidityCoefficient(double coefficient) {
    m_riverHumidityCoefficient = coefficient;
}

double WorldgenSettings::riverHumidityCoefficient() const noexcept {
    return m_riverHumidityCoefficient;
}

void WorldgenSettings::setRiverVegetationCoefficient(double coefficient) {
    m_riverVegetationCoefficient = coefficient;
}

double WorldgenSettings::riverVegetationCoefficient() const noexcept {
    return m_riverVegetationCoefficient;
}

void WorldgenSettings::setProvinceStartScore(double score) {
    m_provinceStartScore = score;
}

double WorldgenSettings::provinceStartScore() const noexcept {
    return m_provinceStartScore;
}

void WorldgenSettings::setProvinceRiverContribution(double contribution) {
    m_provinceRiverContribution = contribution;
}

double WorldgenSettings::provinceRiverContribution() const noexcept {
    return m_provinceRiverContribution;
}

void WorldgenSettings::setProvinceElevationContribution(double contribution) {
    m_provinceElevationContribution = contribution;
}

double WorldgenSettings::provinceElevationContribution() const noexcept {
    return m_provinceElevationContribution;
}

void WorldgenSettings::setProvinceDistanceContribution(double contribution) {
    m_provinceDistanceContribution = contribution;
}

double WorldgenSettings::provinceDistanceContribution() const noexcept {
    return m_provinceDistanceContribution;
}

void WorldgenSettings::setProvinceLandTypeContribution(double contribution) {
    m_provinceLandTypeContribution = contribution;
}

double WorldgenSettings::provinceLandTypeContribution() const noexcept {
    return m_provinceLandTypeContribution;
}

void WorldgenSettings::setProvinceShortBorderContribution(
    double contribution) {
    m_provinceShortBorderContribution = contribution;
}

double WorldgenSettings::provinceShortBorderContribution() const noexcept {
    return m_provinceShortBorderContribution;
}

void WorldgenSettings::setProvinceBaseCost(double cost) {
    m_provinceBaseCost = cost;
}

double WorldgenSettings::provinceBaseCost() const noexcept {
    return m_provinceBaseCost;
}

void WorldgenSettings::setProvinceMinimumRegionCount(std::int64_t count) {
    m_provinceMinimumRegionCount = count;
}

std::int64_t WorldgenSettings::provinceMinimumRegionCount() const noexcept {
    return m_provinceMinimumRegionCount;
}

} // namespace worldgen
