#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <cstdint>

namespace worldgen {

class WorldgenSettings final : public godot::Resource {
    GDCLASS(WorldgenSettings, godot::Resource)

public:
    WorldgenSettings() = default;

    void setBounds(const godot::Rect2 &bounds);
    [[nodiscard]] godot::Rect2 bounds() const;

    void setSeed(std::int64_t seed);
    [[nodiscard]] std::int64_t seed() const noexcept;

    void setColumns(std::int64_t columns);
    [[nodiscard]] std::int64_t columns() const noexcept;

    void setRows(std::int64_t rows);
    [[nodiscard]] std::int64_t rows() const noexcept;

    void setJitter(double jitter);
    [[nodiscard]] double jitter() const noexcept;

    void setSeaLevel(double seaLevel);
    [[nodiscard]] double seaLevel() const noexcept;

    void setEdgeDecayRatio(const godot::Vector2 &ratio);
    [[nodiscard]] godot::Vector2 edgeDecayRatio() const;

    void setEdgeStrength(double strength);
    [[nodiscard]] double edgeStrength() const noexcept;

    void setNoiseOctaves(std::int64_t octaves);
    [[nodiscard]] std::int64_t noiseOctaves() const noexcept;

    void setNoiseFrequency(double frequency);
    [[nodiscard]] double noiseFrequency() const noexcept;

    void setNoiseLacunarity(double lacunarity);
    [[nodiscard]] double noiseLacunarity() const noexcept;

    void setNoisePersistence(double persistence);
    [[nodiscard]] double noisePersistence() const noexcept;

    void setEquatorTemperature(double temperature);
    [[nodiscard]] double equatorTemperature() const noexcept;

    void setPoleTemperature(double temperature);
    [[nodiscard]] double poleTemperature() const noexcept;

    void setVegetationCoefficient(double coefficient);
    [[nodiscard]] double vegetationCoefficient() const noexcept;

    void setHumidityCoefficient(double coefficient);
    [[nodiscard]] double humidityCoefficient() const noexcept;

    void setTemperatureNoiseStrength(double strength);
    [[nodiscard]] double temperatureNoiseStrength() const noexcept;

    void setTemperatureNoiseFrequency(double frequency);
    [[nodiscard]] double temperatureNoiseFrequency() const noexcept;

    void setTemperatureElevationCooling(double cooling);
    [[nodiscard]] double temperatureElevationCooling() const noexcept;

    void setTemperatureHumidityInfluence(double influence);
    [[nodiscard]] double temperatureHumidityInfluence() const noexcept;

    void setTemperatureLatitudeExponent(double exponent);
    [[nodiscard]] double temperatureLatitudeExponent() const noexcept;

    void setOceanHumidityCoefficient(double coefficient);
    [[nodiscard]] double oceanHumidityCoefficient() const noexcept;

    void setOceanHumidityDistanceRatio(double ratio);
    [[nodiscard]] double oceanHumidityDistanceRatio() const noexcept;

    void setLandTypeSnowTemperature(double temperature);
    [[nodiscard]] double landTypeSnowTemperature() const noexcept;

    void setLandTypeColdTemperature(double temperature);
    [[nodiscard]] double landTypeColdTemperature() const noexcept;

    void setLandTypeHotTemperature(double temperature);
    [[nodiscard]] double landTypeHotTemperature() const noexcept;

    void setLandTypeDryHumidity(double humidity);
    [[nodiscard]] double landTypeDryHumidity() const noexcept;

    void setLandTypeWetHumidity(double humidity);
    [[nodiscard]] double landTypeWetHumidity() const noexcept;

    void setLandTypeSparseVegetation(double vegetation);
    [[nodiscard]] double landTypeSparseVegetation() const noexcept;

    void setLandTypeLushVegetation(double vegetation);
    [[nodiscard]] double landTypeLushVegetation() const noexcept;

    void setLandTypeLowlandElevation(double elevation);
    [[nodiscard]] double landTypeLowlandElevation() const noexcept;

    void setLandTypeHillElevation(double elevation);
    [[nodiscard]] double landTypeHillElevation() const noexcept;

    void setLandTypeMountainElevation(double elevation);
    [[nodiscard]] double landTypeMountainElevation() const noexcept;

    void setRiverSourceCount(std::int64_t count);
    [[nodiscard]] std::int64_t riverSourceCount() const noexcept;

    void setRiverMinimumSourceElevation(double elevation);
    [[nodiscard]] double riverMinimumSourceElevation() const noexcept;

    void setRiverRandomness(double randomness);
    [[nodiscard]] double riverRandomness() const noexcept;

    void setRiverElevationTolerance(double tolerance);
    [[nodiscard]] double riverElevationTolerance() const noexcept;

    void setRiverHumidityCoefficient(double coefficient);
    [[nodiscard]] double riverHumidityCoefficient() const noexcept;

    void setRiverVegetationCoefficient(double coefficient);
    [[nodiscard]] double riverVegetationCoefficient() const noexcept;

    void setProvinceStartScore(double score);
    [[nodiscard]] double provinceStartScore() const noexcept;

    void setProvinceRiverContribution(double contribution);
    [[nodiscard]] double provinceRiverContribution() const noexcept;

    void setProvinceElevationContribution(double contribution);
    [[nodiscard]] double provinceElevationContribution() const noexcept;

    void setProvinceDistanceContribution(double contribution);
    [[nodiscard]] double provinceDistanceContribution() const noexcept;

    void setProvinceShortBorderContribution(double contribution);
    [[nodiscard]] double provinceShortBorderContribution() const noexcept;

    void setProvinceBaseCost(double cost);
    [[nodiscard]] double provinceBaseCost() const noexcept;

    void setProvinceMinimumRegionCount(std::int64_t count);
    [[nodiscard]] std::int64_t provinceMinimumRegionCount() const noexcept;

protected:
    static void _bind_methods();

private:
    godot::Rect2 m_bounds{{0.0, 0.0}, {2048.0, 2048.0}};
    std::int64_t m_seed{0};
    std::int64_t m_columns{100};
    std::int64_t m_rows{100};
    double m_jitter{0.8};
    double m_seaLevel{0.45};
    godot::Vector2 m_edgeDecayRatio{0.15, 0.15};
    double m_edgeStrength{0.55};
    std::int64_t m_noiseOctaves{5};
    double m_noiseFrequency{0.01};
    double m_noiseLacunarity{2.0};
    double m_noisePersistence{0.5};
    double m_equatorTemperature{30.0};
    double m_poleTemperature{-20.0};
    double m_vegetationCoefficient{1.0};
    double m_humidityCoefficient{1.0};
    double m_temperatureNoiseStrength{8.0};
    double m_temperatureNoiseFrequency{0.003};
    double m_temperatureElevationCooling{20.0};
    double m_temperatureHumidityInfluence{4.0};
    double m_temperatureLatitudeExponent{1.0};
    double m_oceanHumidityCoefficient{0.2};
    double m_oceanHumidityDistanceRatio{0.12};
    double m_landTypeSnowTemperature{0.0};
    double m_landTypeColdTemperature{6.0};
    double m_landTypeHotTemperature{20.0};
    double m_landTypeDryHumidity{0.45};
    double m_landTypeWetHumidity{0.62};
    double m_landTypeSparseVegetation{0.45};
    double m_landTypeLushVegetation{0.54};
    double m_landTypeLowlandElevation{0.18};
    double m_landTypeHillElevation{0.38};
    double m_landTypeMountainElevation{0.68};
    std::int64_t m_riverSourceCount{12};
    double m_riverMinimumSourceElevation{0.6};
    double m_riverRandomness{0.25};
    double m_riverElevationTolerance{0.03};
    double m_riverHumidityCoefficient{0.05};
    double m_riverVegetationCoefficient{0.05};
    double m_provinceStartScore{10.0};
    double m_provinceRiverContribution{5.0};
    double m_provinceElevationContribution{10.0};
    double m_provinceDistanceContribution{5.0};
    double m_provinceShortBorderContribution{5.0};
    double m_provinceBaseCost{1.0};
    std::int64_t m_provinceMinimumRegionCount{3};
};

} // namespace worldgen
