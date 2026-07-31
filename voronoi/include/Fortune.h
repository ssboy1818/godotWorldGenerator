#pragma once

#include "BeachLine.h"
#include "DCEL.h"
#include "EventQueue.h"

class Fortune {
public:
    Fortune() noexcept = default;

    void calculateVoronoi(const std::vector<Site> &sites);

    const DCEL &dcel() const noexcept;

private:
    BeachLine m_beachline;
    DCEL m_dcel;
    EventQueue m_events;


private:
    void addSiteEvents(const std::vector<Site> &sites);

    void handleSiteEvent(SiteEvent *event);
    void handleCircleEvent(CircleEvent *event);
};
