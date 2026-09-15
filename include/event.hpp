#pragma once

#include <compare>
#include <cstdint>
#include <string>

namespace scheduler {

struct EventKey {
    std::int64_t timestamp{};
    std::uint64_t id{};

    auto operator<=>(const EventKey&) const = default;
};

struct Event {
    EventKey key{};
    std::string title;

    bool operator==(const Event&) const = default;
};

}  // namespace scheduler

