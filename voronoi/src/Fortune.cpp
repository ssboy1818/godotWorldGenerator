#include "Fortune.h"

void Fortune::calculateVoronoi(const std::vector<Site> &sites) {
    addSiteEvents(sites);

    while (!m_events.empty()) {
        auto *event = m_events.pop();
        if (event->type() == EventType::Site)
            handleSiteEvent(static_cast<SiteEvent *>(event));
        else
            handleCircleEvent(static_cast<CircleEvent *>(event));

        // checkNewCircleEvents();
    }
}

const DCEL &Fortune::dcel() const noexcept {
    return m_dcel;
}

void Fortune::addSiteEvents(const std::vector<Site> &sites) {
    for (const auto &site : sites) {
        const auto siteId = m_dcel.addSite(site.position);
        auto *event = new SiteEvent(siteId, m_dcel.site(siteId).position);
        m_events.add(event);
    }
}

void Fortune::handleSiteEvent(SiteEvent *event) {
    if (m_beachline.arcs().empty())
        m_beachline.insert(Arc{event->site()});
}

void Fortune::handleCircleEvent(CircleEvent *event) {

}
