#include "binary_search_tree_event_set.hpp"
#include "red_black_tree_event_set.hpp"
#include "sorted_vector_event_set.hpp"

#include <iomanip>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using scheduler::BinarySearchTreeEventSet;
using scheduler::Event;
using scheduler::EventKey;
using scheduler::OrderedEventSet;
using scheduler::RedBlackTreeEventSet;
using scheduler::SortedVectorEventSet;

void demonstrate(std::string_view name, OrderedEventSet& event_set) {
    const std::vector<Event> events{
        {{1'730'000'000, 1}, "Algorithms lecture"},
        {{1'730'003'600, 2}, "Team meeting"},
        {{1'730'001'800, 3}, "Assignment checkpoint"},
        {{1'730'003'600, 4}, "Library booking"},
    };

    for (const auto& event : events) {
        event_set.insert(event);
    }

    std::cout << "\n" << name << "\n";
    std::cout << "timestamp     id  title\n";
    for (const auto& event : event_set.ordered_events()) {
        std::cout << std::setw(10) << event.key.timestamp << "  "
                  << std::setw(3) << event.key.id << "  " << event.title << "\n";
    }

    const Event* next = event_set.lower_bound(EventKey{1'730'002'000, 0});
    std::cout << "first event at/after 1730002000: "
              << (next == nullptr ? "none" : next->title) << "\n";
    std::cout << "size=" << event_set.size() << ", height=" << event_set.height()
              << ", estimated bytes=" << event_set.estimated_memory_bytes() << "\n";
}

}  // namespace

int main() {
    std::vector<std::pair<std::string_view, std::unique_ptr<OrderedEventSet>>> implementations;
    implementations.emplace_back("Red-black tree", std::make_unique<RedBlackTreeEventSet>());
    implementations.emplace_back("Ordinary BST", std::make_unique<BinarySearchTreeEventSet>());
    implementations.emplace_back("Sorted vector", std::make_unique<SortedVectorEventSet>());

    for (auto& [name, implementation] : implementations) {
        demonstrate(name, *implementation);
    }
}

