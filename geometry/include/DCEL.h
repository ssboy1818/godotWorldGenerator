#pragma once

#include "Edge.h"
#include "Polygon.h"
#include "Site.h"
#include "Vertex.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class DCEL {
public:
    SiteId addSite(Vector2d position) {
        const auto id = nextId(m_sites);
        m_sites.emplace_back(position);
        m_sites.back().id = id;
        return id;
    }

    VertexId addVertex(Vector2d position) {
        const auto id = nextId(m_vertices);
        m_vertices.push_back({id, position, INVALID_ID});
        return id;
    }

    PolygonId addPolygon(SiteId site = INVALID_ID) {
        if (site != INVALID_ID)
            this->site(site);

        const auto id = nextId(m_polygons);
        m_polygons.push_back({id, site, INVALID_ID});
        return id;
    }

    EdgeId addEdge(VertexId origin = INVALID_ID, PolygonId face = INVALID_ID) {
        Vertex *originVertex = nullptr;
        if (origin != INVALID_ID)
            originVertex = &vertex(origin);
        if (face != INVALID_ID)
            polygon(face);

        const auto id = nextId(m_edges);
        m_edges.emplace_back(origin, face);
        m_edges.back().id = id;

        if (originVertex != nullptr && originVertex->edge == INVALID_ID)
            originVertex->edge = id;

        return id;
    }

    std::pair<EdgeId, EdgeId> addEdgePair(VertexId firstOrigin,
                                          VertexId secondOrigin,
                                          PolygonId firstFace = INVALID_ID,
                                          PolygonId secondFace = INVALID_ID) {
        const auto first = addEdge(firstOrigin, firstFace);
        const auto second = addEdge(secondOrigin, secondFace);
        setTwins(first, second);
        return {first, second};
    }

    std::pair<EdgeId, EdgeId> addEdgePairForFaces(PolygonId firstFace,
                                                   PolygonId secondFace) {
        const auto first = addEdge(INVALID_ID, firstFace);
        const auto second = addEdge(INVALID_ID, secondFace);
        setTwins(first, second);
        return {first, second};
    }

    void setOrigin(EdgeId edgeId, VertexId origin) {
        auto &targetEdge = edge(edgeId);
        auto &originVertex = vertex(origin);
        if (targetEdge.origin != INVALID_ID && targetEdge.origin != origin)
            throw std::logic_error("The edge origin is already set.");

        targetEdge.origin = origin;
        if (originVertex.edge == INVALID_ID)
            originVertex.edge = edgeId;
    }

    void setTwins(EdgeId first, EdgeId second) {
        if (first == second)
            throw std::invalid_argument("An edge cannot be its own twin.");

        edge(first).twin = second;
        edge(second).twin = first;
    }

    void linkEdges(EdgeId previous, EdgeId next) {
        edge(previous).next = next;
        edge(next).prev = previous;
    }

    void setPolygonBoundary(PolygonId polygonId, EdgeId boundary) {
        polygon(polygonId).edge = boundary;
        edge(boundary).face = polygonId;
    }

    Site &site(SiteId id) {
        return get(m_sites, id, "site");
    }

    const Site &site(SiteId id) const {
        return get(m_sites, id, "site");
    }

    Vertex &vertex(VertexId id) {
        return get(m_vertices, id, "vertex");
    }

    const Vertex &vertex(VertexId id) const {
        return get(m_vertices, id, "vertex");
    }

    Edge &edge(EdgeId id) {
        return get(m_edges, id, "edge");
    }

    const Edge &edge(EdgeId id) const {
        return get(m_edges, id, "edge");
    }

    Polygon &polygon(PolygonId id) {
        return get(m_polygons, id, "polygon");
    }

    const Polygon &polygon(PolygonId id) const {
        return get(m_polygons, id, "polygon");
    }

    const std::vector<Site> &sites() const noexcept {
        return m_sites;
    }

    const std::vector<Vertex> &vertices() const noexcept {
        return m_vertices;
    }

    const std::vector<Edge> &edges() const noexcept {
        return m_edges;
    }

    const std::vector<Polygon> &polygons() const noexcept {
        return m_polygons;
    }

    void clear() noexcept {
        m_sites.clear();
        m_vertices.clear();
        m_edges.clear();
        m_polygons.clear();
    }

private:
    std::vector<Site> m_sites;
    std::vector<Vertex> m_vertices;
    std::vector<Edge> m_edges;
    std::vector<Polygon> m_polygons;

private:
    template <class T>
    static ssize_t nextId(const std::vector<T> &values) noexcept {
        return static_cast<ssize_t>(values.size());
    }

    template <class T>
    static T &get(std::vector<T> &values, ssize_t id, const char *type) {
        if (id < 0 || static_cast<std::size_t>(id) >= values.size())
            throw std::out_of_range(std::string("Invalid ") + type + " ID.");

        return values[static_cast<std::size_t>(id)];
    }

    template <class T>
    static const T &get(const std::vector<T> &values, ssize_t id, const char *type) {
        if (id < 0 || static_cast<std::size_t>(id) >= values.size())
            throw std::out_of_range(std::string("Invalid ") + type + " ID.");

        return values[static_cast<std::size_t>(id)];
    }
};
