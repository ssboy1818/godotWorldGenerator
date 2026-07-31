cmake_minimum_required(VERSION 3.16)

project(worldgen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(geometry)
add_subdirectory(voronoi)
add_subdirectory(noise)

add_executable(worldgen main.cpp)
target_link_libraries(worldgen PRIVATE worldgen::voronoi)

include(GNUInstallDirs)
install(TARGETS worldgen
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
