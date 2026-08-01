add_executable(worldgen_generation_tests
    WorldGenerationTests.cpp
)

target_link_libraries(worldgen_generation_tests
    PRIVATE
        worldgen::terrain
)

target_compile_features(worldgen_generation_tests PRIVATE cxx_std_23)

add_test(NAME worldgen_generation_tests COMMAND worldgen_generation_tests)

add_executable(worldgen_climate_tests
    ClimateTests.cpp
)

target_link_libraries(worldgen_climate_tests
    PRIVATE
        worldgen::climate
)

target_compile_features(worldgen_climate_tests PRIVATE cxx_std_23)

add_test(NAME worldgen_climate_tests COMMAND worldgen_climate_tests)

add_executable(worldgen_land_type_tests
    LandTypeTests.cpp
)

target_link_libraries(worldgen_land_type_tests
    PRIVATE
        worldgen::terrain
)

target_compile_features(worldgen_land_type_tests PRIVATE cxx_std_23)

add_test(NAME worldgen_land_type_tests COMMAND worldgen_land_type_tests)
