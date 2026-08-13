add_library(worldgen_terrain STATIC
    src/JitteredGridSiteGenerator.cpp
    src/Landform.cpp
    src/LandType.cpp
    src/Province.cpp
    src/ProvinceGenerator.cpp
    src/RiverGenerator.cpp
    src/WorldGenerator.cpp
)
add_library(worldgen::terrain ALIAS worldgen_terrain)

target_include_directories(worldgen_terrain
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(worldgen_terrain
    PUBLIC
        worldgen::climate
        worldgen::noise
        worldgen::voronoi
)

target_compile_features(worldgen_terrain PUBLIC cxx_std_23)
