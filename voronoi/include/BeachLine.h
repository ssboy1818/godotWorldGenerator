#pragma once

#include "Arc.h"
#include <set>

class BeachLine {
public:
    BeachLine() noexcept = default;

    void insert(Arc arc);

    std::set<Arc> &arcs() noexcept;

private:
    std::set<Arc> m_arcs;
};

inline void BeachLine::insert(Arc arc) {
    m_arcs.insert(arc);
}

inline std::set<Arc> &BeachLine::arcs() noexcept {
    return m_arcs;
}
