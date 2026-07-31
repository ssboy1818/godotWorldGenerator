#include "Fortune.h"

#include "BeachLine.h"
#include "DCEL.h"
#include "EventQueue.h"
#include "NumericalPolicy.h"
#include "SiteValidation.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

double halfPlaneValue(Vector2d point,
                      Vector2d normal,
                      Vector2d midpoint) noexcept {
    const auto relative = point - midpoint;
    return relative.x * normal.x + relative.y * normal.y;
}

std::vector<Vector2d> clipPolygon(const std::vector<Vector2d> &input,
                                  Vector2d normal,
                                  Vector2d midpoint,
                                  double lengthTolerance) {
    std::vector<Vector2d> output;
    if (input.empty())
        return output;

    output.reserve(input.size() + 1);
    const auto halfPlaneTolerance = lengthTolerance * normal.length();
    auto previous = input.back();
    auto previousValue = halfPlaneValue(previous, normal, midpoint);
    auto previousInside = previousValue <= halfPlaneTolerance;

    for (const auto &current : input) {
        const auto currentValue = halfPlaneValue(current, normal, midpoint);
        const auto currentInside = currentValue <= halfPlaneTolerance;

        if (currentInside != previousInside) {
            const auto amount = previousValue / (previousValue - currentValue);
            output.push_back(previous + (current - previous) * amount);
        }
        if (currentInside)
            output.push_back(current);

        previous = current;
        previousValue = currentValue;
        previousInside = currentInside;
    }

    return output;
}

class NearestSiteIndex {
public:
    explicit NearestSiteIndex(const std::vector<Site> &sites)
        : m_sites(sites), m_order(sites.size()) {
        std::iota(m_order.begin(), m_order.end(), SiteId{0});
        build(0, m_order.size(), 0);
    }

    [[nodiscard]] std::vector<SiteId> nearest(SiteId target,
                                               std::size_t count) const {
        CandidateHeap candidates;
        search(0,
               m_order.size(),
               0,
               target,
               m_sites[static_cast<std::size_t>(target)].position,
               count,
               candidates);

        std::vector<SiteId> result;
        result.reserve(candidates.size());
        while (!candidates.empty()) {
            result.push_back(candidates.top().second);
            candidates.pop();
        }
        return result;
    }

private:
    using Candidate = std::pair<double, SiteId>;
    using CandidateHeap = std::priority_queue<Candidate>;

    const std::vector<Site> &m_sites;
    std::vector<SiteId> m_order;

    [[nodiscard]] double coordinate(SiteId site, std::size_t axis) const noexcept {
        const auto &position = m_sites[static_cast<std::size_t>(site)].position;
        return axis == 0 ? position.x : position.y;
    }

    void build(std::size_t begin, std::size_t end, std::size_t depth) {
        if (begin >= end)
            return;

        const auto middle = begin + (end - begin) / 2;
        const auto axis = depth % 2;
        std::nth_element(
            m_order.begin() + static_cast<std::ptrdiff_t>(begin),
            m_order.begin() + static_cast<std::ptrdiff_t>(middle),
            m_order.begin() + static_cast<std::ptrdiff_t>(end),
            [this, axis](SiteId left, SiteId right) {
                const auto leftCoordinate = coordinate(left, axis);
                const auto rightCoordinate = coordinate(right, axis);
                if (leftCoordinate != rightCoordinate)
                    return leftCoordinate < rightCoordinate;
                return left < right;
            });
        build(begin, middle, depth + 1);
        build(middle + 1, end, depth + 1);
    }

    void search(std::size_t begin,
                std::size_t end,
                std::size_t depth,
                SiteId target,
                Vector2d position,
                std::size_t count,
                CandidateHeap &candidates) const {
        if (begin >= end || count == 0)
            return;

        const auto middle = begin + (end - begin) / 2;
        const auto site = m_order[middle];
        const auto axis = depth % 2;
        const auto delta = (axis == 0 ? position.x : position.y) - coordinate(site, axis);
        const auto nearBegin = delta < 0.0 ? begin : middle + 1;
        const auto nearEnd = delta < 0.0 ? middle : end;
        const auto farBegin = delta < 0.0 ? middle + 1 : begin;
        const auto farEnd = delta < 0.0 ? end : middle;

        search(nearBegin,
               nearEnd,
               depth + 1,
               target,
               position,
               count,
               candidates);

        if (site != target) {
            const auto distance = (m_sites[static_cast<std::size_t>(site)].position - position)
                                      .squaredLength();
            if (candidates.size() < count) {
                candidates.emplace(distance, site);
            } else if (distance < candidates.top().first) {
                candidates.pop();
                candidates.emplace(distance, site);
            }
        }

        if (candidates.size() < count || delta * delta <= candidates.top().first) {
            search(farBegin,
                   farEnd,
                   depth + 1,
                   target,
                   position,
                   count,
                   candidates);
        }
    }
};

std::vector<std::vector<SiteId>> collectCandidateNeighbors(const DCEL &diagram) {
    std::vector<std::vector<SiteId>> candidates(diagram.polygons().size());

    // Face pairs are reliable sweep output even though the DCEL's linked boundary
    // cycles are intentionally incomplete. They are candidates only; adjacency is
    // confirmed later against the final clipped polygons.
    for (const auto &edge : diagram.edges()) {
        if (edge.twin == INVALID_ID || edge.id > edge.twin || edge.face == INVALID_ID)
            continue;

        const auto &twin = diagram.edge(edge.twin);
        if (twin.face == INVALID_ID || twin.face == edge.face)
            continue;

        const auto &firstCell = diagram.polygon(edge.face);
        const auto &secondCell = diagram.polygon(twin.face);
        candidates[static_cast<std::size_t>(firstCell.id)].push_back(secondCell.site);
        candidates[static_cast<std::size_t>(secondCell.id)].push_back(firstCell.site);
    }

    constexpr std::size_t supplementalNeighborCount = 12;
    const NearestSiteIndex siteIndex{diagram.sites()};
    for (const auto &cell : diagram.polygons()) {
        auto nearest = siteIndex.nearest(cell.site, supplementalNeighborCount);
        auto &cellCandidates = candidates[static_cast<std::size_t>(cell.id)];
        cellCandidates.insert(cellCandidates.end(), nearest.begin(), nearest.end());
    }

    for (auto &cellCandidates : candidates) {
        std::ranges::sort(cellCandidates);
        const auto duplicates = std::ranges::unique(cellCandidates);
        cellCandidates.erase(duplicates.begin(), duplicates.end());
    }

    // Supplemental nearest-site candidates are directional. Make the candidate
    // relation symmetric before clipping and final shared-edge verification.
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        for (const auto candidate : candidates[index]) {
            if (candidate == static_cast<SiteId>(index))
                continue;
            candidates[static_cast<std::size_t>(candidate)].push_back(
                static_cast<SiteId>(index));
        }
    }
    for (auto &cellCandidates : candidates) {
        std::ranges::sort(cellCandidates);
        const auto duplicates = std::ranges::unique(cellCandidates);
        cellCandidates.erase(duplicates.begin(), duplicates.end());
    }

    return candidates;
}

void snapToBoundingBox(Vector2d &point,
                       const BoundingBox &boundingBox,
                       double tolerance) noexcept {
    if (almostEqual(point.x, boundingBox.min.x, tolerance))
        point.x = boundingBox.min.x;
    else if (almostEqual(point.x, boundingBox.max.x, tolerance))
        point.x = boundingBox.max.x;

    if (almostEqual(point.y, boundingBox.min.y, tolerance))
        point.y = boundingBox.min.y;
    else if (almostEqual(point.y, boundingBox.max.y, tolerance))
        point.y = boundingBox.max.y;
}

void cleanPolygon(std::vector<Vector2d> &vertices,
                  const BoundingBox &boundingBox,
                  double tolerance) {
    std::vector<Vector2d> cleaned;
    cleaned.reserve(vertices.size());

    for (auto point : vertices) {
        snapToBoundingBox(point, boundingBox, tolerance);
        if (cleaned.empty()
            || !pointsAlmostEqual(cleaned.back(), point, tolerance)) {
            cleaned.push_back(point);
        }
    }
    if (cleaned.size() > 1
        && pointsAlmostEqual(cleaned.front(), cleaned.back(), tolerance)) {
        cleaned.pop_back();
    }

    // Removing a short edge can expose another short edge at the join.
    bool removed = true;
    while (removed && cleaned.size() >= 3) {
        removed = false;
        for (std::size_t index = 0; index < cleaned.size(); ++index) {
            const auto next = (index + 1) % cleaned.size();
            if (!pointsAlmostEqual(cleaned[index], cleaned[next], tolerance))
                continue;

            cleaned.erase(cleaned.begin() + static_cast<std::ptrdiff_t>(next));
            removed = true;
            break;
        }
    }

    long double twiceArea = 0.0L;
    if (!cleaned.empty()) {
        const auto origin = cleaned.front();
        for (std::size_t index = 1; index + 1 < cleaned.size(); ++index) {
            const auto current = cleaned[index] - origin;
            const auto next = cleaned[index + 1] - origin;
            twiceArea += static_cast<long double>(current.x) * next.y
                         - static_cast<long double>(current.y) * next.x;
        }
    }
    if (twiceArea < 0.0L)
        std::ranges::reverse(cleaned);

    vertices = std::move(cleaned);
}

bool segmentsShareBoundary(Vector2d firstStart,
                           Vector2d firstEnd,
                           Vector2d secondStart,
                           Vector2d secondEnd,
                           double tolerance) noexcept {
    const auto firstDirection = firstEnd - firstStart;
    const auto secondDirection = secondEnd - secondStart;
    const auto firstLength = firstDirection.length();
    const auto secondLength = secondDirection.length();
    if (firstLength <= tolerance || secondLength <= tolerance)
        return false;

    const Vector2d unit{firstDirection.x / firstLength,
                        firstDirection.y / firstLength};
    const auto perpendicularDistance = [&](Vector2d point) {
        const auto relative = point - firstStart;
        return std::abs(relative.x * unit.y - relative.y * unit.x);
    };
    if (perpendicularDistance(secondStart) > tolerance
        || perpendicularDistance(secondEnd) > tolerance) {
        return false;
    }

    const auto project = [&](Vector2d point) {
        const auto relative = point - firstStart;
        return relative.x * unit.x + relative.y * unit.y;
    };
    auto secondMinimum = project(secondStart);
    auto secondMaximum = project(secondEnd);
    if (secondMinimum > secondMaximum)
        std::swap(secondMinimum, secondMaximum);

    const auto overlap = std::min(firstLength, secondMaximum)
                         - std::max(0.0, secondMinimum);
    return overlap > tolerance;
}

bool polygonsShareBoundary(const std::vector<Vector2d> &first,
                           const std::vector<Vector2d> &second,
                           double tolerance) noexcept {
    for (std::size_t firstIndex = 0; firstIndex < first.size(); ++firstIndex) {
        const auto firstNext = (firstIndex + 1) % first.size();
        for (std::size_t secondIndex = 0; secondIndex < second.size(); ++secondIndex) {
            const auto secondNext = (secondIndex + 1) % second.size();
            if (segmentsShareBoundary(first[firstIndex],
                                      first[firstNext],
                                      second[secondIndex],
                                      second[secondNext],
                                      tolerance)) {
                return true;
            }
        }
    }
    return false;
}

WorldDivision finishBoundedCells(const DCEL &diagram,
                                 const BoundingBox &boundingBox,
                                 NumericalTolerance tolerance) {
    const auto candidates = collectCandidateNeighbors(diagram);
    const auto cellCount = diagram.polygons().size();
    WorldDivision division;
    division.cells.reserve(cellCount);

    for (std::size_t index = 0; index < cellCount; ++index) {
        const auto cellId = static_cast<PolygonId>(index);
        const auto siteId = diagram.polygon(cellId).site;
        const auto &site = diagram.site(siteId);
        std::vector<Vector2d> boundary{
            boundingBox.min,
            {boundingBox.max.x, boundingBox.min.y},
            boundingBox.max,
            {boundingBox.min.x, boundingBox.max.y},
        };

        for (const auto candidate : candidates[index]) {
            const auto &other = diagram.site(candidate);
            const auto normal = other.position - site.position;
            const auto midpoint = site.position + normal * 0.5;
            boundary = clipPolygon(boundary,
                                   normal,
                                   midpoint,
                                   tolerance.geometryLength);
        }

        cleanPolygon(boundary, boundingBox, tolerance.geometryLength);
        if (boundary.size() < 3) {
            throw std::logic_error(
                "Fortune produced a degenerate clipped polygon for site "
                + std::to_string(index) + ".");
        }

        division.cells.push_back({
            static_cast<CellId>(index),
            site.position,
            std::move(boundary),
            {},
        });
    }

    const auto sharedEdgeTolerance = tolerance.sharedEdgeLength();
    for (std::size_t index = 0; index < cellCount; ++index) {
        for (const auto candidate : candidates[index]) {
            const auto otherIndex = static_cast<std::size_t>(candidate);
            if (otherIndex <= index)
                continue;
            if (!polygonsShareBoundary(division.cells[index].vertices,
                                       division.cells[otherIndex].vertices,
                                       sharedEdgeTolerance)) {
                continue;
            }

            division.cells[index].neighbors.push_back(
                static_cast<CellId>(otherIndex));
            division.cells[otherIndex].neighbors.push_back(
                static_cast<CellId>(index));
        }
    }
    for (auto &cell : division.cells)
        std::ranges::sort(cell.neighbors);

    return division;
}

class FortuneSweep {
public:
    [[nodiscard]] WorldDivision run(std::span<const Site> sites,
                                    const BoundingBox &boundingBox,
                                    NumericalTolerance tolerance);

private:
    BeachLine m_beachline;
    DCEL m_dcel;
    EventQueue m_events;
    std::vector<PolygonId> m_siteFaces;

    void addSiteEvents(std::span<const Site> sites);
    void handleSiteEvent(SiteEvent *event);
    void handleCircleEvent(CircleEvent *event);
    void checkCircleEvent(BeachLine::Node *node, double sweepLine);
    void invalidateCircleEvent(BeachLine::Node *node);
    [[nodiscard]] PolygonId faceFor(SiteId site) const;
    [[nodiscard]] std::pair<EdgeId, EdgeId> createEdgePair(
        BeachLine::Node *left,
        BeachLine::Node *right);
    void setFaceBoundary(PolygonId face, EdgeId edge);
};

WorldDivision FortuneSweep::run(std::span<const Site> sites,
                                const BoundingBox &boundingBox,
                                NumericalTolerance tolerance) {
    m_events.clear();
    m_beachline.clear();
    m_dcel.clear();
    m_siteFaces.clear();

    addSiteEvents(sites);

    while (!m_events.empty()) {
        auto *event = m_events.pop();
        if (event->type() == EventType::Site)
            handleSiteEvent(static_cast<SiteEvent *>(event));
        else
            handleCircleEvent(static_cast<CircleEvent *>(event));
    }

    return finishBoundedCells(m_dcel, boundingBox, tolerance);
}

void FortuneSweep::addSiteEvents(std::span<const Site> sites) {
    for (const auto &site : sites) {
        const auto siteId = m_dcel.addSite(site.position);
        m_siteFaces.push_back(m_dcel.addPolygon(siteId));
        m_events.add(std::make_unique<SiteEvent>(siteId, m_dcel.site(siteId).position));
    }
}

void FortuneSweep::handleSiteEvent(SiteEvent *event) {
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

void FortuneSweep::handleCircleEvent(CircleEvent *event) {
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

void FortuneSweep::checkCircleEvent(BeachLine::Node *node, double sweepLine) {
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

void FortuneSweep::invalidateCircleEvent(BeachLine::Node *node) {
    if (node == nullptr)
        return;

    auto *event = m_beachline.takePendingEvent(node);
    if (event != nullptr)
        event->setInvalid();
}

PolygonId FortuneSweep::faceFor(SiteId site) const {
    if (site < 0 || static_cast<std::size_t>(site) >= m_siteFaces.size())
        throw std::out_of_range("Site does not belong to this Fortune sweep.");

    return m_siteFaces[static_cast<std::size_t>(site)];
}

std::pair<EdgeId, EdgeId> FortuneSweep::createEdgePair(BeachLine::Node *left,
                                                        BeachLine::Node *right) {
    const auto leftFace = faceFor(left->arc().focus);
    const auto rightFace = faceFor(right->arc().focus);
    const auto edges = m_dcel.addEdgePairForFaces(leftFace, rightFace);
    setFaceBoundary(leftFace, edges.first);
    setFaceBoundary(rightFace, edges.second);
    return edges;
}

void FortuneSweep::setFaceBoundary(PolygonId face, EdgeId edge) {
    if (m_dcel.polygon(face).edge == INVALID_ID)
        m_dcel.setPolygonBoundary(face, edge);
}

} // namespace

WorldDivision Fortune::generate(std::span<const Site> sites,
                                const BoundingBox &boundingBox) const {
    const auto tolerance = validateSites(sites, boundingBox);
    FortuneSweep sweep;
    return sweep.run(sites, boundingBox, tolerance);
}
