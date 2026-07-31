#pragma once

#include "Event.h"

#include <queue>
#include <stdexcept>

class EventQueue {
public:
    EventQueue() noexcept = default;

    bool empty() const noexcept;

    Event *pop();

    void add(Event *event);

private:
    std::priority_queue<Event *> m_queue;
};

inline bool EventQueue::empty() const noexcept {
    return m_queue.empty();
}

inline Event *EventQueue::pop() {
    if (empty())
        throw std::runtime_error("Очередь пуста.");

    auto event = m_queue.top();
    m_queue.pop();

    return event;
}

inline void EventQueue::add(Event *event) {
    m_queue.push(event);
}
