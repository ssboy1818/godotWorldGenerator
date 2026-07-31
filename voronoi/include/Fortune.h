#pragma once

#include "BeachLine.h"
#include "DCEL.h"
#include "EventQueue.h"

#include <utility>
#include <vector>

class Fortune {
public:
    Fortune() noexcept = default;

    void calculateVoronoi(const std::vector<Site> &sites);

    const DCEL &dcel() const noexcept;

private:
    BeachLine m_beachline;
    DCEL m_dcel;
    EventQueue m_events;
    std::vector<PolygonId> m_siteFaces;

private:
    void addSiteEvents(const std::vector<Site> &sites);

    void handleSiteEvent(SiteEvent *event);
    void handleCircleEvent(CircleEvent *event);

    void checkCircleEvent(BeachLine::Node *node, double sweepLine);
    void invalidateCircleEvent(BeachLine::Node *node);

    PolygonId faceFor(SiteId site) const;
    std::pair<EdgeId, EdgeId> createEdgePair(BeachLine::Node *left,
                                             BeachLine::Node *right);
    void setFaceBoundary(PolygonId face, EdgeId edge);
};
