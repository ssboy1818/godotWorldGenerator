#include "JitteredGridSiteGenerator.h"
#include "WorldGenerator.h"

#ifndef NDEBUG
#include <chrono>
#include <print>
#endif

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>

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
        const auto &cell = world.diagram().polygon(region.cell());
        if (cell.vertices.size() < 3)
            continue;

        output << "    <polygon fill=\""
               << (region.isWater() ? "#38bdf8" : "#84cc16")
               << "\" points=\"";
        for (const auto vertex : cell.vertices) {
            const auto &point = world.diagram().vertex(vertex).position;
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
    const BoundingBox boundingBox{{0.0, 0.0}, {1024.0, 1024.0}};
    auto siteGenerator = std::make_unique<JitteredGridSiteGenerator>(300, 300, 0.8);
    WorldGenerator worldGenerator{std::move(siteGenerator)};

#ifndef NDEBUG
    const auto start = std::chrono::steady_clock::now();
#endif
    const auto world = worldGenerator.generate(boundingBox);
#ifndef NDEBUG
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start);
    const auto &dcel = world.diagram();
    const auto width = boundingBox.max.x - boundingBox.min.x;
    const auto height = boundingBox.max.y - boundingBox.min.y;
    const auto edgeCount = dcel.edges().size() / 2;
    const auto timePerSite = dcel.sites().empty()
                                 ? 0.0
                                 : elapsed.count() / static_cast<double>(dcel.sites().size());
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
               "Edges: {}\n"
               "Vertices: {}\n"
               "Time consumed: {:.3f} ms\n"
               "Time per site: {:.3f} ms\n",
               width,
               height,
               dcel.sites().size(),
               world.regions().size() - static_cast<std::size_t>(waterCount),
               waterCount,
               edgeCount,
               dcel.vertices().size(),
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
