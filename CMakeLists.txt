cmake_minimum_required(VERSION 3.16)

project(worldgen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(worldgen main.cpp
    geometry/include/Vector2.h)

include(GNUInstallDirs)
install(TARGETS worldgen
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
