#pragma once

#include "ordered_event_set.hpp"

#include <utility>

namespace scheduler {

class RedBlackTreeEventSet final : public OrderedEventSet {
public:
    RedBlackTreeEventSet();
    ~RedBlackTreeEventSet() override;

    RedBlackTreeEventSet(const RedBlackTreeEventSet&) = delete;
    RedBlackTreeEventSet& operator=(const RedBlackTreeEventSet&) = delete;
    RedBlackTreeEventSet(RedBlackTreeEventSet&&) = delete;
    RedBlackTreeEventSet& operator=(RedBlackTreeEventSet&&) = delete;

    bool insert(Event event) override;
    bool erase(EventKey key) override;
    [[nodiscard]] const Event* find(EventKey key) const override;
    [[nodiscard]] const Event* lower_bound(EventKey key) const override;
    [[nodiscard]] std::vector<Event> ordered_events() const override;
    [[nodiscard]] std::size_t size() const noexcept override;
    [[nodiscard]] std::size_t height() const override;
    [[nodiscard]] std::size_t estimated_memory_bytes() const noexcept override;
    void clear() noexcept override;

    [[nodiscard]] bool validate_invariants() const noexcept;

private:
    enum class Color { red, black };

    struct Node {
        Node(Event value, Color node_color, Node* nil)
            : event(std::move(value)), color(node_color), parent(nil), left(nil), right(nil) {}

        Event event;
        Color color{Color::black};
        Node* parent{};
        Node* left{};
        Node* right{};
    };

    Node* nil_{};
    Node* root_{};
    std::size_t size_{};

    [[nodiscard]] Node* find_node(EventKey key) const noexcept;
    [[nodiscard]] Node* minimum(Node* node) const noexcept;
    void left_rotate(Node* node) noexcept;
    void right_rotate(Node* node) noexcept;
    void insert_fixup(Node* node) noexcept;
    void erase_fixup(Node* node) noexcept;
    void transplant(Node* old_node, Node* replacement) noexcept;
    [[nodiscard]] int validate_subtree(
        const Node* node,
        const EventKey* minimum_key,
        const EventKey* maximum_key,
        std::size_t& visited) const noexcept;
};

}  // namespace scheduler
