#include "binary_search_tree_event_set.hpp"
#include "red_black_tree_event_set.hpp"
#include "sorted_vector_event_set.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

#ifndef RANDOM_OPERATION_COUNT
#define RANDOM_OPERATION_COUNT 10000
#endif

using scheduler::BinarySearchTreeEventSet;
using scheduler::Event;
using scheduler::EventKey;
using scheduler::OrderedEventSet;
using scheduler::RedBlackTreeEventSet;
using scheduler::SortedVectorEventSet;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void require_matches_oracle(const OrderedEventSet& subject, const std::map<EventKey, Event>& oracle) {
    const auto actual = subject.ordered_events();
    require(actual.size() == oracle.size(), "ordered result has wrong size");
    std::size_t index = 0;
    for (const auto& [key, event] : oracle) {
        require(actual[index].key == key, "ordered result has wrong key");
        require(actual[index] == event, "ordered result has wrong event");
        ++index;
    }
}

void common_behavior_suite(std::string_view name, std::unique_ptr<OrderedEventSet> subject) {
    require(subject->empty(), "new set should be empty");
    require(subject->height() == 0, "empty set should have zero height");
    require(subject->find({10, 1}) == nullptr, "empty find should miss");
    require(subject->lower_bound({10, 0}) == nullptr, "empty lower_bound should miss");
    require(!subject->erase({10, 1}), "empty erase should fail");

    const std::vector<Event> events{
        {{20, 2}, "twenty-b"},
        {{10, 1}, "ten"},
        {{30, 1}, "thirty"},
        {{20, 1}, "twenty-a"},
        {{std::numeric_limits<std::int64_t>::min(), 9}, "minimum"},
        {{std::numeric_limits<std::int64_t>::max(), 9}, "maximum"},
    };
    for (const auto& event : events) {
        require(subject->insert(event), "first insert should succeed");
    }
    require(!subject->insert({{20, 1}, "replacement"}), "duplicate insert should fail");
    require(subject->size() == events.size(), "duplicate changed size");
    require(subject->find({20, 1})->title == "twenty-a", "duplicate overwrote event");

    const Event* lower = subject->lower_bound({20, 0});
    require(lower != nullptr && lower->key == EventKey{20, 1}, "lower_bound chose wrong event");
    lower = subject->lower_bound({20, 2});
    require(lower != nullptr && lower->key == EventKey{20, 2}, "exact lower_bound failed");
    lower = subject->lower_bound({21, 0});
    require(lower != nullptr && lower->key == EventKey{30, 1}, "gap lower_bound failed");

    std::map<EventKey, Event> oracle;
    for (const auto& event : events) {
        oracle.emplace(event.key, event);
    }
    require_matches_oracle(*subject, oracle);
    require(subject->estimated_memory_bytes() >= sizeof(*subject), "invalid memory estimate");

    for (const EventKey key : {EventKey{10, 1}, EventKey{20, 2}, EventKey{30, 1}}) {
        require(subject->erase(key), "existing erase failed");
        oracle.erase(key);
        require(subject->find(key) == nullptr, "erased event was found");
        require_matches_oracle(*subject, oracle);
    }
    require(!subject->erase({123, 456}), "missing erase should fail");

    subject->clear();
    require(subject->empty(), "clear did not empty set");
    require(subject->ordered_events().empty(), "clear left ordered events");
    subject->clear();
    std::cout << "[pass] " << name << " common behavior\n";
}

template <typename Set>
void randomized_oracle_suite(std::string_view name) {
    Set subject;
    std::map<EventKey, Event> oracle;
    std::mt19937_64 generator(0x41052U);
    std::uniform_int_distribution<std::int64_t> timestamp_distribution(-100, 100);
    std::uniform_int_distribution<std::uint64_t> id_distribution(0, 30);
    std::uniform_int_distribution<int> operation_distribution(0, 2);

    for (int step = 0; step < RANDOM_OPERATION_COUNT; ++step) {
        const EventKey key{timestamp_distribution(generator), id_distribution(generator)};
        const int operation = operation_distribution(generator);
        if (operation == 0) {
            Event event{key, "event-" + std::to_string(step)};
            const bool expected = oracle.emplace(key, event).second;
            require(subject.insert(std::move(event)) == expected, "random insert disagreed with oracle");
        } else if (operation == 1) {
            const bool expected = oracle.erase(key) != 0;
            require(subject.erase(key) == expected, "random erase disagreed with oracle");
        } else {
            const auto expected = oracle.find(key);
            const Event* actual = subject.find(key);
            require((actual != nullptr) == (expected != oracle.end()), "random find disagreed with oracle");
            if (actual != nullptr) {
                require(*actual == expected->second, "random find returned wrong event");
            }

            const auto expected_lower = oracle.lower_bound(key);
            const Event* actual_lower = subject.lower_bound(key);
            require((actual_lower != nullptr) == (expected_lower != oracle.end()),
                    "random lower_bound disagreed with oracle");
            if (actual_lower != nullptr) {
                require(*actual_lower == expected_lower->second, "random lower_bound returned wrong event");
            }
        }

        if constexpr (std::is_same_v<Set, RedBlackTreeEventSet>) {
            require(subject.validate_invariants(), "red-black invariant failed during random operations");
        }
        if (step % 100 == 0) {
            require_matches_oracle(subject, oracle);
        }
    }
    require_matches_oracle(subject, oracle);
    std::cout << "[pass] " << name << " randomized oracle\n";
}

void structural_suite() {
    constexpr std::size_t count = 1'000;
    BinarySearchTreeEventSet bst;
    RedBlackTreeEventSet red_black_tree;
    for (std::size_t index = 0; index < count; ++index) {
        const auto timestamp = static_cast<std::int64_t>(index);
        const auto id = static_cast<std::uint64_t>(index);
        require(bst.insert({{timestamp, id}, "event"}), "BST sorted insert failed");
        require(red_black_tree.insert({{timestamp, id}, "event"}), "RBT sorted insert failed");
        require(red_black_tree.validate_invariants(), "RBT invariant failed after sorted insert");
    }

    require(bst.height() == count, "sorted input should make BST linear");
    const auto red_black_height_limit = static_cast<std::size_t>(
        std::floor(2.0 * std::log2(static_cast<double>(count + 1))));
    require(red_black_tree.height() <= red_black_height_limit, "RBT exceeded theoretical height bound");

    for (std::size_t index = 0; index < count; index += 2) {
        const EventKey key{static_cast<std::int64_t>(index), static_cast<std::uint64_t>(index)};
        require(red_black_tree.erase(key), "RBT sorted erase failed");
        require(red_black_tree.validate_invariants(), "RBT invariant failed after sorted erase");
    }
    std::cout << "[pass] structural/adversarial behavior\n";
}

}  // namespace

int main() {
    try {
        common_behavior_suite("red-black tree", std::make_unique<RedBlackTreeEventSet>());
        common_behavior_suite("ordinary BST", std::make_unique<BinarySearchTreeEventSet>());
        common_behavior_suite("sorted vector", std::make_unique<SortedVectorEventSet>());

        randomized_oracle_suite<RedBlackTreeEventSet>("red-black tree");
        randomized_oracle_suite<BinarySearchTreeEventSet>("ordinary BST");
        randomized_oracle_suite<SortedVectorEventSet>("sorted vector");
        structural_suite();
    } catch (const std::exception& error) {
        std::cerr << "[fail] " << error.what() << "\n";
        return 1;
    }

    std::cout << "All tests passed.\n";
    return 0;
}
