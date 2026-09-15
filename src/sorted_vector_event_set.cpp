#include "sorted_vector_event_set.hpp"

#include <algorithm>
#include <utility>

namespace scheduler {

bool SortedVectorEventSet::insert(Event event) {
    const auto position = lower_bound_iterator(event.key);
    if (position != events_.end() && position->key == event.key) {
        return false;
    }
    events_.insert(position, std::move(event));
    return true;
}

bool SortedVectorEventSet::erase(EventKey key) {
    const auto position = lower_bound_iterator(key);
    if (position == events_.end() || position->key != key) {
        return false;
    }
    events_.erase(position);
    return true;
}

const Event* SortedVectorEventSet::find(EventKey key) const {
    const auto position = lower_bound_iterator(key);
    return position != events_.end() && position->key == key ? &*position : nullptr;
}

const Event* SortedVectorEventSet::lower_bound(EventKey key) const {
    const auto position = lower_bound_iterator(key);
    return position == events_.end() ? nullptr : &*position;
}

std::vector<Event> SortedVectorEventSet::ordered_events() const {
    return events_;
}

std::size_t SortedVectorEventSet::size() const noexcept {
    return events_.size();
}

std::size_t SortedVectorEventSet::height() const {
    return 0;
}

std::size_t SortedVectorEventSet::estimated_memory_bytes() const noexcept {
    std::size_t bytes = sizeof(*this) + events_.capacity() * sizeof(Event);
    for (const auto& event : events_) {
        bytes += event.title.capacity() + 1;
    }
    return bytes;
}

void SortedVectorEventSet::clear() noexcept {
    events_.clear();
}

SortedVectorEventSet::Iterator SortedVectorEventSet::lower_bound_iterator(EventKey key) {
    return std::lower_bound(events_.begin(), events_.end(), key, [](const Event& event, EventKey value) {
        return event.key < value;
    });
}

SortedVectorEventSet::ConstIterator SortedVectorEventSet::lower_bound_iterator(EventKey key) const {
    return std::lower_bound(events_.begin(), events_.end(), key, [](const Event& event, EventKey value) {
        return event.key < value;
    });
}

}  // namespace scheduler
