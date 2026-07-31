#pragma once

#include "Event.h"

#include <memory>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace worldgen {

class EventQueue {
public:
    EventQueue() noexcept = default;

    [[nodiscard]] bool empty() const noexcept {
        return m_queue.empty();
    }

    Event *pop() {
        if (empty())
            throw std::runtime_error("Event queue is empty.");

        auto *event = m_queue.top();
        m_queue.pop();
        return event;
    }

    Event *add(std::unique_ptr<Event> event) {
        if (event == nullptr)
            throw std::invalid_argument("An event is required.");

        auto *rawEvent = event.get();
        m_events.push_back(std::move(event));
        m_queue.push(rawEvent);
        return rawEvent;
    }

    void clear() noexcept {
        m_queue = {};
        m_events.clear();
    }

private:
    struct EventCompare {
        bool operator()(const Event *lhs, const Event *rhs) const noexcept {
            return *lhs < *rhs;
        }
    };

    std::priority_queue<Event *, std::vector<Event *>, EventCompare> m_queue;
    std::vector<std::unique_ptr<Event>> m_events;
};

} // namespace worldgen
