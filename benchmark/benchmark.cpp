#include "binary_search_tree_event_set.hpp"
#include "red_black_tree_event_set.hpp"
#include "sorted_vector_event_set.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
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

constexpr std::string_view csv_header =
    "scenario,distribution,implementation,n,trial,seed,operations,elapsed_ns,ns_per_op,height,"
    "estimated_bytes,checksum";

enum class Implementation { red_black_tree, ordinary_bst, sorted_vector };
enum class Distribution { sorted, random, nearly_sorted };
enum class OperationType { insert, erase, find };

struct Options {
    std::filesystem::path output{"results/raw.csv"};
    std::uint64_t seed{41052};
    int repetitions{7};
    bool quick{false};
};

struct Operation {
    OperationType type{};
    Event event{};
    EventKey key{};
};

struct Result {
    std::string_view scenario;
    std::string_view distribution;
    std::string_view implementation;
    std::size_t n{};
    int trial{};
    std::uint64_t seed{};
    std::size_t operations{};
    std::uint64_t elapsed_ns{};
    double ns_per_op{};
    std::size_t height{};
    bool height_applicable{};
    std::size_t estimated_bytes{};
    std::uint64_t checksum{};
};

[[nodiscard]] std::string_view implementation_name(Implementation implementation) {
    switch (implementation) {
        case Implementation::red_black_tree:
            return "red_black_tree";
        case Implementation::ordinary_bst:
            return "ordinary_bst";
        case Implementation::sorted_vector:
            return "sorted_vector";
    }
    throw std::logic_error("unknown implementation");
}

[[nodiscard]] std::string_view distribution_name(Distribution distribution) {
    switch (distribution) {
        case Distribution::sorted:
            return "sorted";
        case Distribution::random:
            return "random";
        case Distribution::nearly_sorted:
            return "nearly_sorted";
    }
    throw std::logic_error("unknown distribution");
}

[[nodiscard]] std::unique_ptr<OrderedEventSet> make_set(Implementation implementation) {
    switch (implementation) {
        case Implementation::red_black_tree:
            return std::make_unique<RedBlackTreeEventSet>();
        case Implementation::ordinary_bst:
            return std::make_unique<BinarySearchTreeEventSet>();
        case Implementation::sorted_vector:
            return std::make_unique<SortedVectorEventSet>();
    }
    throw std::logic_error("unknown implementation");
}

[[nodiscard]] std::uint64_t mix_checksum(std::uint64_t checksum, std::uint64_t value) noexcept {
    constexpr std::uint64_t golden_ratio = 0x9e3779b97f4a7c15ULL;
    return checksum ^ (value + golden_ratio + (checksum << 6U) + (checksum >> 2U));
}

[[nodiscard]] std::vector<Event> make_events(std::size_t n) {
    std::vector<Event> events;
    events.reserve(n);
    for (std::size_t index = 0; index < n; ++index) {
        const auto timestamp = static_cast<std::int64_t>(index * 2U);
        const auto id = static_cast<std::uint64_t>(index);
        events.push_back({{timestamp, id}, "event"});
    }
    return events;
}

[[nodiscard]] std::vector<std::size_t> make_order(
    std::size_t n,
    Distribution distribution,
    std::uint64_t seed) {
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::mt19937_64 generator(seed);

    if (distribution == Distribution::random) {
        std::shuffle(order.begin(), order.end(), generator);
    } else if (distribution == Distribution::nearly_sorted && n > 1) {
        std::uniform_int_distribution<std::size_t> position(0, n - 2);
        for (std::size_t swap_index = 0; swap_index < n / 20U; ++swap_index) {
            const std::size_t first = position(generator);
            std::swap(order[first], order[first + 1]);
        }
    }
    return order;
}

void populate(OrderedEventSet& set, const std::vector<Event>& events, const std::vector<std::size_t>& order) {
    for (const std::size_t index : order) {
        if (!set.insert(events[index])) {
            throw std::runtime_error("benchmark setup inserted a duplicate event");
        }
    }
}

void verify_set(
    const OrderedEventSet& set,
    const std::map<EventKey, Event>& expected,
    Implementation implementation) {
    const std::vector<Event> actual = set.ordered_events();
    if (actual.size() != expected.size()) {
        throw std::runtime_error("post-trial size mismatch");
    }
    std::size_t index = 0;
    for (const auto& [key, event] : expected) {
        if (actual[index].key != key || actual[index] != event) {
            throw std::runtime_error("post-trial content mismatch");
        }
        ++index;
    }
    if (implementation == Implementation::red_black_tree) {
        const auto& tree = static_cast<const RedBlackTreeEventSet&>(set);
        if (!tree.validate_invariants()) {
            throw std::runtime_error("red-black invariants failed after benchmark trial");
        }
    }
}

[[nodiscard]] Result finish_result(
    std::string_view scenario,
    std::string_view distribution,
    Implementation implementation,
    std::size_t n,
    int trial,
    std::uint64_t seed,
    std::size_t operation_count,
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point finish,
    const OrderedEventSet& set,
    std::uint64_t checksum) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
    if (elapsed < 0) {
        throw std::runtime_error("steady clock returned a negative duration");
    }
    const auto elapsed_ns = static_cast<std::uint64_t>(elapsed);
    return {
        scenario,
        distribution,
        implementation_name(implementation),
        n,
        trial,
        seed,
        operation_count,
        elapsed_ns,
        static_cast<double>(elapsed_ns) / static_cast<double>(operation_count),
        set.height(),
        implementation != Implementation::sorted_vector,
        set.estimated_memory_bytes(),
        checksum,
    };
}

[[nodiscard]] Result run_build(
    Implementation implementation,
    Distribution distribution,
    std::size_t n,
    int trial,
    std::uint64_t seed) {
    const auto events = make_events(n);
    const auto order = make_order(n, distribution, seed);
    auto set = make_set(implementation);

    const auto start = std::chrono::steady_clock::now();
    std::uint64_t checksum = 0;
    for (const std::size_t index : order) {
        checksum = mix_checksum(checksum, set->insert(events[index]) ? 1U : 0U);
    }
    const auto finish = std::chrono::steady_clock::now();

    std::map<EventKey, Event> expected;
    for (const auto& event : events) {
        expected.emplace(event.key, event);
    }
    verify_set(*set, expected, implementation);
    checksum = mix_checksum(checksum, static_cast<std::uint64_t>(set->size()));
    return finish_result(
        "build", distribution_name(distribution), implementation, n, trial, seed, n, start, finish, *set, checksum);
}

[[nodiscard]] std::vector<EventKey> make_lookup_queries(
    const std::vector<Event>& events,
    std::uint64_t seed) {
    std::vector<EventKey> queries;
    queries.reserve(events.size());
    for (std::size_t index = 0; index < events.size(); ++index) {
        if (index % 2U == 0) {
            queries.push_back(events[index].key);
        } else {
            queries.push_back({events[index].key.timestamp + 1, events[index].key.id});
        }
    }
    std::mt19937_64 generator(seed);
    std::shuffle(queries.begin(), queries.end(), generator);
    return queries;
}

[[nodiscard]] Result run_lookup(
    Implementation implementation,
    Distribution distribution,
    std::size_t n,
    int trial,
    std::uint64_t seed) {
    const auto events = make_events(n);
    const auto order = make_order(n, distribution, seed);
    const auto queries = make_lookup_queries(events, seed ^ 0xa5a5a5a5ULL);
    auto set = make_set(implementation);
    populate(*set, events, order);

    const auto start = std::chrono::steady_clock::now();
    std::uint64_t checksum = 0;
    for (const EventKey query : queries) {
        const Event* found = set->find(query);
        checksum = mix_checksum(checksum, found == nullptr ? 0U : found->key.id + 1U);
    }
    const auto finish = std::chrono::steady_clock::now();

    std::map<EventKey, Event> expected;
    for (const auto& event : events) {
        expected.emplace(event.key, event);
    }
    verify_set(*set, expected, implementation);
    return finish_result(
        "lookup", distribution_name(distribution), implementation, n, trial, seed, queries.size(), start, finish,
        *set, checksum);
}

[[nodiscard]] std::vector<Operation> make_mixed_trace(
    const std::vector<Event>& initial,
    int find_percent,
    int insert_percent,
    std::uint64_t seed,
    std::map<EventKey, Event>& expected) {
    const std::size_t operation_count = initial.size() * 2U;
    const std::size_t find_count = operation_count * static_cast<std::size_t>(find_percent) / 100U;
    const std::size_t insert_count = operation_count * static_cast<std::size_t>(insert_percent) / 100U;
    const std::size_t erase_count = operation_count - find_count - insert_count;

    std::vector<OperationType> types;
    types.insert(types.end(), find_count, OperationType::find);
    types.insert(types.end(), insert_count, OperationType::insert);
    types.insert(types.end(), erase_count, OperationType::erase);
    std::mt19937_64 generator(seed);
    std::shuffle(types.begin(), types.end(), generator);

    std::vector<EventKey> live_keys;
    live_keys.reserve(initial.size() + insert_count);
    for (const auto& event : initial) {
        expected.emplace(event.key, event);
        live_keys.push_back(event.key);
    }

    std::vector<Operation> trace;
    trace.reserve(operation_count);
    std::uint64_t next_id = static_cast<std::uint64_t>(initial.size());
    std::int64_t next_timestamp = static_cast<std::int64_t>(initial.size() * 2U);
    std::uint64_t miss_id = 1;

    for (const OperationType type : types) {
        if (type == OperationType::insert) {
            Event event{{next_timestamp, next_id}, "event"};
            trace.push_back({type, event, event.key});
            expected.emplace(event.key, event);
            live_keys.push_back(event.key);
            ++next_id;
            next_timestamp += 2;
        } else if (type == OperationType::erase) {
            if (live_keys.empty()) {
                throw std::runtime_error("mixed trace unexpectedly exhausted live keys");
            }
            std::uniform_int_distribution<std::size_t> choose(0, live_keys.size() - 1U);
            const std::size_t position = choose(generator);
            const EventKey key = live_keys[position];
            trace.push_back({type, {}, key});
            expected.erase(key);
            live_keys[position] = live_keys.back();
            live_keys.pop_back();
        } else {
            const bool choose_hit = !live_keys.empty() && (generator() % 2U == 0U);
            if (choose_hit) {
                std::uniform_int_distribution<std::size_t> choose(0, live_keys.size() - 1U);
                trace.push_back({type, {}, live_keys[choose(generator)]});
            } else {
                trace.push_back({type, {}, {-static_cast<std::int64_t>(miss_id), miss_id}});
                ++miss_id;
            }
        }
    }
    return trace;
}

[[nodiscard]] Result run_mixed(
    Implementation implementation,
    bool read_heavy,
    std::size_t n,
    int trial,
    std::uint64_t seed) {
    const auto events = make_events(n);
    const auto order = make_order(n, Distribution::random, seed);
    std::map<EventKey, Event> expected;
    const auto trace = make_mixed_trace(events, read_heavy ? 80 : 20, read_heavy ? 10 : 40, seed, expected);
    auto set = make_set(implementation);
    populate(*set, events, order);

    const auto start = std::chrono::steady_clock::now();
    std::uint64_t checksum = 0;
    for (const auto& operation : trace) {
        switch (operation.type) {
            case OperationType::insert:
                checksum = mix_checksum(checksum, set->insert(operation.event) ? 1U : 0U);
                break;
            case OperationType::erase:
                checksum = mix_checksum(checksum, set->erase(operation.key) ? 1U : 0U);
                break;
            case OperationType::find: {
                const Event* found = set->find(operation.key);
                checksum = mix_checksum(checksum, found == nullptr ? 0U : found->key.id + 1U);
                break;
            }
        }
    }
    const auto finish = std::chrono::steady_clock::now();

    verify_set(*set, expected, implementation);
    return finish_result(
        read_heavy ? "read_heavy" : "update_heavy", "random", implementation, n, trial, seed, trace.size(),
        start, finish, *set, checksum);
}

void write_result(std::ostream& output, const Result& result) {
    output << result.scenario << ',' << result.distribution << ',' << result.implementation << ',' << result.n
           << ',' << result.trial << ',' << result.seed << ',' << result.operations << ',' << result.elapsed_ns
           << ',' << std::fixed << std::setprecision(6) << result.ns_per_op << ',';
    if (result.height_applicable) {
        output << result.height;
    }
    output << ',' << result.estimated_bytes << ',' << result.checksum << '\n';
}

[[nodiscard]] std::uint64_t trial_seed(std::uint64_t base, std::size_t n, int trial, std::uint64_t salt) {
    return base ^ (static_cast<std::uint64_t>(n) << 20U) ^ (static_cast<std::uint64_t>(trial + 2) << 8U) ^ salt;
}

template <typename Runner>
void run_point(
    std::ofstream& output,
    const std::vector<Implementation>& implementations,
    int repetitions,
    std::uint64_t base_seed,
    std::size_t n,
    std::uint64_t salt,
    Runner&& runner,
    std::size_t& rows_written) {
    const std::uint64_t warmup_seed = trial_seed(base_seed, n, -1, salt);
    for (const Implementation implementation : implementations) {
        (void)runner(implementation, -1, warmup_seed);
    }
    for (int trial = 0; trial < repetitions; ++trial) {
        const std::uint64_t seed = trial_seed(base_seed, n, trial, salt);
        std::vector<Implementation> order = implementations;
        std::mt19937_64 generator(seed ^ 0x1ee7c0deULL);
        std::shuffle(order.begin(), order.end(), generator);
        std::uint64_t expected_checksum = 0;
        bool first = true;
        for (const Implementation implementation : order) {
            const Result result = runner(implementation, trial, seed);
            if (!first && result.checksum != expected_checksum) {
                throw std::runtime_error("implementations produced different checksums");
            }
            expected_checksum = result.checksum;
            first = false;
            write_result(output, result);
            ++rows_written;
        }
    }
}

[[nodiscard]] Options parse_options(int argc, char** argv) {
    Options options;
    bool repetitions_was_set = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        auto require_value = [&](std::string_view option) -> std::string_view {
            if (index + 1 >= argc) {
                throw std::invalid_argument(std::string(option) + " requires a value");
            }
            return argv[++index];
        };

        if (argument == "--quick") {
            options.quick = true;
        } else if (argument == "--output") {
            options.output = require_value(argument);
        } else if (argument == "--seed") {
            options.seed = std::stoull(std::string(require_value(argument)));
        } else if (argument == "--repetitions") {
            options.repetitions = std::stoi(std::string(require_value(argument)));
            repetitions_was_set = true;
        } else if (argument == "--help") {
            std::cout << "Usage: benchmark [--quick] [--output PATH] [--seed N] [--repetitions N]\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown option: " + std::string(argument));
        }
    }
    if (options.repetitions <= 0) {
        throw std::invalid_argument("repetitions must be positive");
    }
    if (options.quick && !repetitions_was_set) {
        options.repetitions = 2;
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        const std::vector<std::size_t> sizes =
            options.quick ? std::vector<std::size_t>{100, 250}
                          : std::vector<std::size_t>{250, 500, 1'000, 2'000, 4'000, 8'000};
        const std::vector<Implementation> implementations{
            Implementation::red_black_tree,
            Implementation::ordinary_bst,
            Implementation::sorted_vector,
        };
        const std::vector<Distribution> distributions{
            Distribution::sorted,
            Distribution::random,
            Distribution::nearly_sorted,
        };

        if (!options.output.parent_path().empty()) {
            std::filesystem::create_directories(options.output.parent_path());
        }
        std::ofstream output(options.output);
        if (!output) {
            throw std::runtime_error("could not open output CSV: " + options.output.string());
        }
        output << csv_header << '\n';

        std::size_t rows_written = 0;
        for (const std::size_t n : sizes) {
            for (const Distribution distribution : distributions) {
                const std::uint64_t salt = static_cast<std::uint64_t>(distribution) + 11U;
                run_point(
                    output, implementations, options.repetitions, options.seed, n, salt,
                    [=](Implementation implementation, int trial, std::uint64_t seed) {
                        return run_build(implementation, distribution, n, trial, seed);
                    },
                    rows_written);
                run_point(
                    output, implementations, options.repetitions, options.seed, n, salt + 101U,
                    [=](Implementation implementation, int trial, std::uint64_t seed) {
                        return run_lookup(implementation, distribution, n, trial, seed);
                    },
                    rows_written);
            }

            run_point(
                output, implementations, options.repetitions, options.seed, n, 301U,
                [=](Implementation implementation, int trial, std::uint64_t seed) {
                    return run_mixed(implementation, true, n, trial, seed);
                },
                rows_written);
            run_point(
                output, implementations, options.repetitions, options.seed, n, 401U,
                [=](Implementation implementation, int trial, std::uint64_t seed) {
                    return run_mixed(implementation, false, n, trial, seed);
                },
                rows_written);
        }

        std::cout << "Wrote " << rows_written << " benchmark rows to " << options.output << '\n';
    } catch (const std::exception& error) {
        std::cerr << "benchmark: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
