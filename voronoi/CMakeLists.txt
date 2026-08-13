add_library(worldgen_voronoi STATIC
    src/Fortune.cpp
    src/LloydRelaxer.cpp
    src/SiteValidation.cpp
)
add_library(worldgen::voronoi ALIAS worldgen_voronoi)

target_include_directories(worldgen_voronoi
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(worldgen_voronoi
    PUBLIC
        worldgen::geometry
)

target_compile_features(worldgen_voronoi PUBLIC cxx_std_23)
