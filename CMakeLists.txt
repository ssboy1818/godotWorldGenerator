cmake_minimum_required(VERSION 3.17)

project(worldgen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
