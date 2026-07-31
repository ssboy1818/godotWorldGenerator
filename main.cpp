#include "WorldGenerator.h"

#ifndef NDEBUG
#include <chrono>
#include <print>
#endif

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace {

double svgY(double coordinate, const BoundingBox &boundingBox) {
    return boundingBox.max.y - coordinate;
}

void writeSvg(const World &world, std::ostream &output) {
    const auto &boundingBox = world.boundingBox();
    const auto width = boundingBox.max.x - boundingBox.min.x;
    const auto height = boundingBox.max.y - boundingBox.min.y;

    output << std::fixed << std::setprecision(3);
    output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"800\" "
           << "viewBox=\"" << boundingBox.min.x << " 0 " << width << " " << height << "\">\n";
    output << "  <rect x=\"" << boundingBox.min.x << "\" y=\"0\" width=\"" << width
           << "\" height=\"" << height << "\" fill=\"#38bdf8\"/>\n";
    output << "  <g stroke=\"#334155\" stroke-width=\"1\" stroke-linejoin=\"round\">\n";

    for (const auto &region : world.regions()) {
        const auto &cell = world.division().cells[region.cell()];
        if (cell.vertices.size() < 3)
            continue;

        output << "    <polygon fill=\""
               << (region.isWater() ? "#38bdf8" : "#84cc16")
               << "\" points=\"";
        for (const auto &point : cell.vertices) {
            output << point.x << ',' << svgY(point.y, boundingBox) << ' ';
        }
        output << "\"/>\n";
    }

    output << "  </g>\n";
    output << "  <rect x=\"" << boundingBox.min.x << "\" y=\"0\" width=\"" << width
           << "\" height=\"" << height
           << "\" fill=\"none\" stroke=\"#1e293b\" stroke-width=\"2\"/>\n";
    output << "</svg>\n";
}

}

int main() {
    const WorldGenerator worldGenerator{WorldGenerationSettings{
        .bounds = {{0.0, 0.0}, {2048.0, 2048.0}},
        .seed = 0,
        .columns = 300,
        .rows = 300,
        .jitter = 0.8,
        .seaLevel = 0.45,
        .edgeDecayRatio = {0.15, 0.15},
        .noiseOctaves = 5,
        .noiseFrequency = 0.01,
        .noiseLacunarity = 2.0,
        .noisePersistence = 0.5,
    }};
    const auto &boundingBox = worldGenerator.settings().bounds;

#ifndef NDEBUG
    const auto start = std::chrono::steady_clock::now();
#endif
    const auto world = worldGenerator.generate();
#ifndef NDEBUG
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start);
    const auto &cells = world.division().cells;
    const auto width = boundingBox.max.x - boundingBox.min.x;
    const auto height = boundingBox.max.y - boundingBox.min.y;
    const auto polygonVertexCount = std::accumulate(
        cells.begin(),
        cells.end(),
        std::size_t{0},
        [](std::size_t total, const Cell &cell) {
            return total + cell.vertices.size();
        });
    const auto neighborPairCount = std::accumulate(
        cells.begin(),
        cells.end(),
        std::size_t{0},
        [](std::size_t total, const Cell &cell) {
            return total + cell.neighbors.size();
        }) / 2;
    const auto timePerSite = cells.empty()
                                 ? 0.0
                                 : elapsed.count() / static_cast<double>(cells.size());
    const auto waterCount = std::count_if(world.regions().begin(),
                                          world.regions().end(),
                                          [](const Region &region) {
                                              return region.isWater();
                                          });

    std::print(std::cout,
               "Bounding box: {:.3f} x {:.3f}\n"
               "Sites: {}\n"
               "Land regions: {}\n"
               "Water regions: {}\n"
               "Neighbor pairs: {}\n"
               "Polygon vertices: {}\n"
               "Time consumed: {:.3f} ms\n"
               "Time per site: {:.3f} ms\n",
               width,
               height,
               cells.size(),
               world.regions().size() - static_cast<std::size_t>(waterCount),
               waterCount,
               neighborPairCount,
               polygonVertexCount,
               elapsed.count(),
               timePerSite);
#endif

    std::ofstream output{"voronoi.svg", std::ios::trunc};
    if (!output) {
        std::cerr << "Unable to open voronoi.svg for writing.\n";
        return 1;
    }

    writeSvg(world, output);
    std::cout << "Wrote voronoi.svg\n";
    return 0;
}
