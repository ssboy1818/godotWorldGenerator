#include "Fortune.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {

bool clipAxis(double origin,
              double direction,
              double minimum,
              double maximum,
              double &start,
              double &end) {
    if (std::abs(direction) < EPS)
        return origin >= minimum && origin <= maximum;

    auto first = (minimum - origin) / direction;
    auto second = (maximum - origin) / direction;
    if (first > second)
        std::swap(first, second);

    start = std::max(start, first);
    end = std::min(end, second);
    return start <= end;
}

std::pair<Vector2d, Vector2d> clippedBisector(const Vector2d &left,
                                               const Vector2d &right,
                                               const BoundingBox &boundingBox) {
    const Vector2d midpoint{(left.x + right.x) / 2.0, (left.y + right.y) / 2.0};
    const Vector2d direction{left.y - right.y, right.x - left.x};
    auto start = -std::numeric_limits<double>::infinity();
    auto end = std::numeric_limits<double>::infinity();

    if (!clipAxis(midpoint.x, direction.x, boundingBox.min.x, boundingBox.max.x, start, end)
        || !clipAxis(midpoint.y, direction.y, boundingBox.min.y, boundingBox.max.y, start, end))
        throw std::logic_error("The Voronoi bisector does not intersect the bounding box.");

    return {{midpoint.x + direction.x * start, midpoint.y + direction.y * start},
            {midpoint.x + direction.x * end, midpoint.y + direction.y * end}};
}

}

void Fortune::calculateVoronoi(const std::vector<Site> &sites,
                               const BoundingBox &boundingBox) {
    m_events.clear();
    m_beachline.clear();
    m_dcel.clear();
    m_siteFaces.clear();

    for (const auto &site : sites) {
        if (!boundingBox.contains(site.position))
            throw std::invalid_argument("Every site must lie inside the bounding box.");
    }

    addSiteEvents(sites);

    while (!m_events.empty()) {
        auto *event = m_events.pop();
        if (event->type() == EventType::Site)
            handleSiteEvent(static_cast<SiteEvent *>(event));
        else
            handleCircleEvent(static_cast<CircleEvent *>(event));
    }

    finishOpenEdges(boundingBox);
}

const DCEL &Fortune::dcel() const noexcept {
    return m_dcel;
}

void Fortune::addSiteEvents(const std::vector<Site> &sites) {
    for (const auto &site : sites) {
        const auto siteId = m_dcel.addSite(site.position);
        m_siteFaces.push_back(m_dcel.addPolygon(siteId));
        m_events.add(std::make_unique<SiteEvent>(siteId, m_dcel.site(siteId).position));
    }
}

void Fortune::handleSiteEvent(SiteEvent *event) {
    if (m_beachline.empty()) {
        m_beachline.insertFirst(Arc{event->site()});
        return;
    }

    const auto sweepLine = event->position().y;
    auto *arc = m_beachline.findArcAbove(event->position().x, sweepLine, m_dcel);
    if (arc == nullptr)
        throw std::logic_error("Unable to locate the arc above a site event.");

    auto result = m_beachline.split(arc, Arc{event->site()});
    if (result.invalidatedEvent != nullptr)
        result.invalidatedEvent->setInvalid();

    const auto edges = createEdgePair(result.left, result.middle);
    result.left->arc().rightEdge = edges.first;
    result.middle->arc().rightEdge = edges.second;

    checkCircleEvent(result.left, sweepLine);
    checkCircleEvent(result.middle, sweepLine);
    checkCircleEvent(result.right, sweepLine);
}

void Fortune::handleCircleEvent(CircleEvent *event) {
    if (!event->isValid())
        return;

    auto *arc = m_beachline.nodeFor(event->arc());
    if (arc == nullptr || arc->arc().pendingEvent != event)
        return;

    m_beachline.takePendingEvent(arc);

    auto *left = m_beachline.previous(arc);
    auto *right = m_beachline.next(arc);
    if (left == nullptr || right == nullptr)
        return;

    if (left->arc().focus != event->leftFocus()
        || arc->arc().focus != event->centerFocus()
        || right->arc().focus != event->rightFocus())
        return;

    invalidateCircleEvent(left);
    invalidateCircleEvent(right);

    const auto vertex = m_dcel.addVertex(event->circle().center());
    if (left->arc().rightEdge == INVALID_ID || arc->arc().rightEdge == INVALID_ID)
        throw std::logic_error("Circle event is missing a beach-line edge.");

    const auto leftEdge = left->arc().rightEdge;
    const auto centerEdge = arc->arc().rightEdge;
    m_dcel.setOrigin(leftEdge, vertex);
    m_dcel.setOrigin(centerEdge, vertex);

    const auto edges = createEdgePair(left, right);
    m_dcel.setOrigin(edges.second, vertex);
    m_dcel.linkEdges(leftEdge, edges.first);
    m_dcel.linkEdges(m_dcel.edge(leftEdge).twin, centerEdge);
    m_dcel.linkEdges(m_dcel.edge(centerEdge).twin, edges.second);
    left->arc().rightEdge = edges.first;

    const auto result = m_beachline.erase(arc);
    if (result.invalidatedEvent != nullptr)
        result.invalidatedEvent->setInvalid();

    const auto sweepLine = event->position().y;
    checkCircleEvent(result.previous, sweepLine);
    checkCircleEvent(result.next, sweepLine);
}

void Fortune::checkCircleEvent(BeachLine::Node *node, double sweepLine) {
    if (node == nullptr)
        return;

    invalidateCircleEvent(node);

    auto *left = m_beachline.previous(node);
    auto *right = m_beachline.next(node);
    if (left == nullptr || right == nullptr)
        return;

    const auto &leftPosition = m_dcel.site(left->arc().focus).position;
    const auto &centerPosition = m_dcel.site(node->arc().focus).position;
    const auto &rightPosition = m_dcel.site(right->arc().focus).position;
    const auto orientation = (centerPosition.x - leftPosition.x)
                             * (rightPosition.y - leftPosition.y)
                             - (centerPosition.y - leftPosition.y)
                                   * (rightPosition.x - leftPosition.x);
    if (orientation >= -EPS)
        return;

    std::unique_ptr<CircleEvent> event;
    try {
        event = std::make_unique<CircleEvent>(&node->arc(),
                                              left->arc().focus,
                                              node->arc().focus,
                                              right->arc().focus,
                                              leftPosition,
                                              centerPosition,
                                              rightPosition);
    } catch (const std::invalid_argument &) {
        return;
    }

    if (event->position().y >= sweepLine - EPS)
        return;

    node->arc().pendingEvent = static_cast<CircleEvent *>(m_events.add(std::move(event)));
}

void Fortune::invalidateCircleEvent(BeachLine::Node *node) {
    if (node == nullptr)
        return;

    auto *event = m_beachline.takePendingEvent(node);
    if (event != nullptr)
        event->setInvalid();
}

PolygonId Fortune::faceFor(SiteId site) const {
    if (site < 0 || static_cast<std::size_t>(site) >= m_siteFaces.size())
        throw std::out_of_range("Site does not belong to this Fortune sweep.");

    return m_siteFaces[static_cast<std::size_t>(site)];
}

std::pair<EdgeId, EdgeId> Fortune::createEdgePair(BeachLine::Node *left,
                                                   BeachLine::Node *right) {
    const auto leftFace = faceFor(left->arc().focus);
    const auto rightFace = faceFor(right->arc().focus);
    const auto edges = m_dcel.addEdgePairForFaces(leftFace, rightFace);
    setFaceBoundary(leftFace, edges.first);
    setFaceBoundary(rightFace, edges.second);
    return edges;
}

void Fortune::setFaceBoundary(PolygonId face, EdgeId edge) {
    if (m_dcel.polygon(face).edge == INVALID_ID)
        m_dcel.setPolygonBoundary(face, edge);
}

void Fortune::finishOpenEdges(const BoundingBox &boundingBox) {
    const auto edgeCount = m_dcel.edges().size();
    for (std::size_t index = 0; index < edgeCount; ++index) {
        const auto edge = static_cast<EdgeId>(index);
        if (m_dcel.edge(edge).twin < edge)
            continue;

        finishEdgePair(edge, boundingBox);
    }
}

void Fortune::finishEdgePair(EdgeId edgeId, const BoundingBox &boundingBox) {
    const auto twinId = m_dcel.edge(edgeId).twin;
    if (twinId == INVALID_ID)
        throw std::logic_error("A Voronoi edge must have a twin.");

    const auto &edge = m_dcel.edge(edgeId);
    const auto &twin = m_dcel.edge(twinId);
    if (edge.origin != INVALID_ID && twin.origin != INVALID_ID)
        return;

    const auto &leftSite = m_dcel.site(m_dcel.polygon(edge.face).site).position;
    const auto &rightSite = m_dcel.site(m_dcel.polygon(twin.face).site).position;
    const auto endpoints = clippedBisector(leftSite, rightSite, boundingBox);

    if (edge.origin == INVALID_ID)
        m_dcel.setOrigin(edgeId, boundaryVertex(endpoints.first));
    if (twin.origin == INVALID_ID)
        m_dcel.setOrigin(twinId, boundaryVertex(endpoints.second));
}

VertexId Fortune::boundaryVertex(Vector2d position) {
    for (const auto &vertex : m_dcel.vertices()) {
        if (std::abs(vertex.position.x - position.x) < EPS
            && std::abs(vertex.position.y - position.y) < EPS)
            return vertex.id;
    }

    return m_dcel.addVertex(position);
}
