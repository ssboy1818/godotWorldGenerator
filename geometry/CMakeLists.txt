add_library(worldgen_geometry INTERFACE)
add_library(worldgen::geometry ALIAS worldgen_geometry)

target_include_directories(worldgen_geometry
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(worldgen_geometry INTERFACE cxx_std_23)
