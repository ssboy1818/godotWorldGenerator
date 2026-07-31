#include "Fortune.h"

#ifndef NDEBUG
#include <chrono>
#include <print>
#endif

#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

double svgY(double coordinate, const BoundingBox &boundingBox) {
    return boundingBox.max.y - coordinate;
}

void writeSvg(const DCEL &dcel,
              const BoundingBox &boundingBox,
              std::ostream &output) {
    const auto width = boundingBox.max.x - boundingBox.min.x;
    const auto height = boundingBox.max.y - boundingBox.min.y;

    output << std::fixed << std::setprecision(3);
    output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"800\" "
           << "viewBox=\"" << boundingBox.min.x << " 0 " << width << " " << height << "\">\n";
    output << "  <rect x=\"" << boundingBox.min.x << "\" y=\"0\" width=\"" << width
           << "\" height=\"" << height << "\" fill=\"white\" stroke=\"#1f2937\"/>\n";
    output << "  <g stroke=\"#2563eb\" stroke-width=\"2\">\n";

    for (const auto &edge : dcel.edges()) {
        if (edge.twin == INVALID_ID || edge.id > edge.twin)
            continue;

        const auto &twin = dcel.edge(edge.twin);
        if (edge.origin == INVALID_ID || twin.origin == INVALID_ID)
            continue;

        const auto &start = dcel.vertex(edge.origin).position;
        const auto &end = dcel.vertex(twin.origin).position;
        output << "    <line x1=\"" << start.x << "\" y1=\"" << svgY(start.y, boundingBox)
               << "\" x2=\"" << end.x << "\" y2=\"" << svgY(end.y, boundingBox)
               << "\"/>\n";
    }

    output << "  </g>\n";
    output << "  <g fill=\"#dc2626\" stroke=\"white\" stroke-width=\"2\">\n";

    for (const auto &site : dcel.sites()) {
        output << "    <circle cx=\"" << site.position.x << "\" cy=\""
               << svgY(site.position.y, boundingBox) << "\" r=\"6\"/>\n";
    }

    output << "  </g>\n";
    output << "</svg>\n";
}

}

int main() {
    const BoundingBox boundingBox{{0.0, 0.0}, {1000.0, 1000.0}};
    const std::vector<Site> sites{
        {150.0, 200.0},
        {800.0, 200.0},
        {500.0, 300.0},
    };

    Fortune fortune;
#ifndef NDEBUG
    const auto start = std::chrono::steady_clock::now();
#endif
    fortune.calculateVoronoi(sites, boundingBox);
#ifndef NDEBUG
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start);
    const auto &dcel = fortune.dcel();
    const auto width = boundingBox.max.x - boundingBox.min.x;
    const auto height = boundingBox.max.y - boundingBox.min.y;
    const auto edgeCount = dcel.edges().size() / 2;
    const auto timePerSite = sites.empty() ? 0.0 : elapsed.count() / sites.size();

    std::print(std::cout,
               "Bounding box: {:.3f} x {:.3f}\n"
               "Sites: {}\n"
               "Edges: {}\n"
               "Vertices: {}\n"
               "Time consumed: {:.3f} ms\n"
               "Time per site: {:.3f} ms\n",
               width,
               height,
               sites.size(),
               edgeCount,
               dcel.vertices().size(),
               elapsed.count(),
               timePerSite);
#endif

    std::ofstream output{"voronoi.svg"};
    if (!output) {
        std::cerr << "Unable to open voronoi.svg for writing.\n";
        return 1;
    }

    writeSvg(fortune.dcel(), boundingBox, output);
    std::cout << "Wrote voronoi.svg\n";
    return 0;
}
