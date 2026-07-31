cmake_minimum_required(VERSION 3.16)

project(worldgen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(geometry/include)
include_directories(voronoi/include)

add_executable(worldgen main.cpp
    geometry/include/Vector2.h
    geometry/include/Site.h
    geometry/include/Edge.h
    geometry/include/DCEL.h
    geometry/include/Polygon.h
    geometry/include/Id.h
    geometry/include/Vertex.h
    geometry/include/Circle.h
    voronoi/include/Event.h
    voronoi/include/EventQueue.h
    geometry/include/Arc.h
    voronoi/include/BeachLine.h
    voronoi/include/Fortune.h
    voronoi/src/Fortune.cpp)

include(GNUInstallDirs)
install(TARGETS worldgen
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
