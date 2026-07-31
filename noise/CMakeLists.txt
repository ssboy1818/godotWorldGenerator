add_library(worldgen_noise STATIC
    src/PerlinNoise.cpp
)
add_library(worldgen::noise ALIAS worldgen_noise)

target_include_directories(worldgen_noise
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(worldgen_noise
    PUBLIC
        worldgen::geometry
)

target_compile_features(worldgen_noise PUBLIC cxx_std_23)
