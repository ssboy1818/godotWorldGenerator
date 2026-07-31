cmake_minimum_required(VERSION 3.17)

project(worldgen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(WORLDGEN_GODOT_PROJECT_DIR
    "${PROJECT_SOURCE_DIR}/demo"
    CACHE PATH
    "Directory containing the Worldgen Godot project"
)

add_subdirectory(geometry)
add_subdirectory(voronoi)
add_subdirectory(noise)
add_subdirectory(terrain)

if(NOT DEFINED GODOTCPP_BUILD_PROFILE)
    set(GODOTCPP_BUILD_PROFILE
        "${CMAKE_CURRENT_SOURCE_DIR}/godot/build_profile.json"
        CACHE FILEPATH
        "godot-cpp build profile"
    )
endif()

add_subdirectory(godot-cpp EXCLUDE_FROM_ALL)
add_subdirectory(godot)

find_program(WORLDGEN_GODOT_EXECUTABLE
    NAMES godot4 godot
    DOC "Path to the Godot 4.4 editor executable"
)

if(WORLDGEN_GODOT_EXECUTABLE)
    add_custom_target(worldgen_editor
        COMMAND
            "${WORLDGEN_GODOT_EXECUTABLE}"
            --editor
            --path "${WORLDGEN_GODOT_PROJECT_DIR}"
        DEPENDS worldgen_extension
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(worldgen_smoke
        COMMAND
            "${WORLDGEN_GODOT_EXECUTABLE}"
            --headless
            --path "${WORLDGEN_GODOT_PROJECT_DIR}"
            --script res://smoke_test.gd
        DEPENDS worldgen_extension
        USES_TERMINAL
        VERBATIM
    )
else()
    message(STATUS
        "Godot was not found. Set WORLDGEN_GODOT_EXECUTABLE to a Godot 4.4 "
        "executable to enable the worldgen_editor and worldgen_smoke targets."
    )
endif()
