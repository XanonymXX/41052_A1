#pragma once

#include "ordered_event_set.hpp"

#include <utility>

namespace scheduler {

class BinarySearchTreeEventSet final : public OrderedEventSet {
public:
    BinarySearchTreeEventSet() = default;
    ~BinarySearchTreeEventSet() override;

    BinarySearchTreeEventSet(const BinarySearchTreeEventSet&) = delete;
    BinarySearchTreeEventSet& operator=(const BinarySearchTreeEventSet&) = delete;
    BinarySearchTreeEventSet(BinarySearchTreeEventSet&&) = delete;
    BinarySearchTreeEventSet& operator=(BinarySearchTreeEventSet&&) = delete;

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
    struct Node {
        explicit Node(Event value, Node* parent_node)
            : event(std::move(value)), parent(parent_node) {}

        Event event;
        Node* parent{};
        Node* left{};
        Node* right{};
    };

    Node* root_{};
    std::size_t size_{};

    static Node* minimum(Node* node) noexcept;
    void transplant(Node* old_node, Node* replacement) noexcept;
};

}  // namespace scheduler
