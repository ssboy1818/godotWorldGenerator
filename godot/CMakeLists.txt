add_library(worldgen_extension SHARED)
add_library(worldgen::extension ALIAS worldgen_extension)

target_sources(worldgen_extension
    PRIVATE
        src/RegisterTypes.cpp
        src/VoronoiWorld2D.cpp
        src/VoronoiWorldData.cpp
        src/VoronoiWorldGenerator.cpp
        src/WorldgenSettings.cpp
)

target_include_directories(worldgen_extension
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(worldgen_extension
    PRIVATE
        godot-cpp
        worldgen::terrain
)

target_compile_features(worldgen_extension PRIVATE cxx_std_23)

get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)
get_target_property(GODOTCPP_SYMBOL_VISIBILITY godot-cpp CXX_VISIBILITY_PRESET)

set(WORLDGEN_GODOT_ADDON_DIR
    "${CMAKE_SOURCE_DIR}/demo/addons/worldgen"
    CACHE PATH
    "Directory containing the loadable Worldgen Godot addon"
)
set(WORLDGEN_EXTENSION_OUTPUT_DIR "${WORLDGEN_GODOT_ADDON_DIR}/bin")

set_target_properties(worldgen_extension
    PROPERTIES
        CXX_VISIBILITY_PRESET ${GODOTCPP_SYMBOL_VISIBILITY}
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH_USE_ORIGIN ON
        LIBRARY_OUTPUT_DIRECTORY "$<1:${WORLDGEN_EXTENSION_OUTPUT_DIR}>"
        RUNTIME_OUTPUT_DIRECTORY "$<1:${WORLDGEN_EXTENSION_OUTPUT_DIR}>"
        PDB_OUTPUT_DIRECTORY "$<1:${WORLDGEN_EXTENSION_OUTPUT_DIR}>"
        PREFIX "lib"
        OUTPUT_NAME "worldgen${GODOTCPP_SUFFIX}"
)

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    get_target_property(GODOTCPP_TARGET godot-cpp GODOTCPP_TARGET)
    set_target_properties(worldgen_extension
        PROPERTIES
            SUFFIX ""
            LIBRARY_OUTPUT_DIRECTORY
                "${WORLDGEN_EXTENSION_OUTPUT_DIR}/libworldgen.macos.${GODOTCPP_TARGET}.framework"
    )
endif()

install(TARGETS worldgen_extension
    LIBRARY DESTINATION addons/worldgen/bin
    RUNTIME DESTINATION addons/worldgen/bin
)
install(FILES "${WORLDGEN_GODOT_ADDON_DIR}/worldgen.gdextension"
    DESTINATION addons/worldgen
)
