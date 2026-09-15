#pragma once

#include "event.hpp"

#include <cstddef>
#include <vector>

namespace scheduler {

class OrderedEventSet {
public:
    virtual ~OrderedEventSet() = default;

    virtual bool insert(Event event) = 0;
    virtual bool erase(EventKey key) = 0;

    // Returned pointers remain valid only until the next mutating operation.
    [[nodiscard]] virtual const Event* find(EventKey key) const = 0;
    [[nodiscard]] virtual const Event* lower_bound(EventKey key) const = 0;

    [[nodiscard]] virtual std::vector<Event> ordered_events() const = 0;
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
    [[nodiscard]] virtual std::size_t height() const = 0;
    [[nodiscard]] virtual std::size_t estimated_memory_bytes() const noexcept = 0;

    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    virtual void clear() noexcept = 0;
};

}  // namespace scheduler
