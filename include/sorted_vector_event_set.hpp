#pragma once

#include "ordered_event_set.hpp"

namespace scheduler {

class SortedVectorEventSet final : public OrderedEventSet {
public:
    bool insert(Event event) override;
    bool erase(EventKey key) override;
    [[nodiscard]] const Event* find(EventKey key) const override;
    [[nodiscard]] const Event* lower_bound(EventKey key) const override;
    [[nodiscard]] std::vector<Event> ordered_events() const override;
    [[nodiscard]] std::size_t size() const noexcept override;
    [[nodiscard]] std::size_t height() const override;
    [[nodiscard]] std::size_t estimated_memory_bytes() const noexcept override;
    void clear() noexcept override;

private:
    using Iterator = std::vector<Event>::iterator;
    using ConstIterator = std::vector<Event>::const_iterator;

    [[nodiscard]] Iterator lower_bound_iterator(EventKey key);
    [[nodiscard]] ConstIterator lower_bound_iterator(EventKey key) const;

    std::vector<Event> events_;
};

}  // namespace scheduler
