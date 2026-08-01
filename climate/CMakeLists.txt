add_library(worldgen_climate STATIC
    src/ClimateGenerator.cpp
)
add_library(worldgen::climate ALIAS worldgen_climate)

target_include_directories(worldgen_climate
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(worldgen_climate
    PUBLIC
        worldgen::geometry
        worldgen::noise
)

target_compile_features(worldgen_climate PUBLIC cxx_std_23)
