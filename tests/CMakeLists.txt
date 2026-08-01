add_executable(worldgen_generation_tests
    WorldGenerationTests.cpp
)

target_link_libraries(worldgen_generation_tests
    PRIVATE
        worldgen::terrain
)

target_compile_features(worldgen_generation_tests PRIVATE cxx_std_23)

add_test(NAME worldgen_generation_tests COMMAND worldgen_generation_tests)
