#include "binary_search_tree_event_set.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace scheduler {

BinarySearchTreeEventSet::~BinarySearchTreeEventSet() {
    clear();
}

bool BinarySearchTreeEventSet::insert(Event event) {
    Node* parent = nullptr;
    Node* current = root_;

    while (current != nullptr) {
        parent = current;
        if (event.key < current->event.key) {
            current = current->left;
        } else if (current->event.key < event.key) {
            current = current->right;
        } else {
            return false;
        }
    }

    auto* node = new Node(std::move(event), parent);
    if (parent == nullptr) {
        root_ = node;
    } else if (node->event.key < parent->event.key) {
        parent->left = node;
    } else {
        parent->right = node;
    }
    ++size_;
    return true;
}

bool BinarySearchTreeEventSet::erase(EventKey key) {
    Node* node = root_;
    while (node != nullptr && node->event.key != key) {
        node = key < node->event.key ? node->left : node->right;
    }
    if (node == nullptr) {
        return false;
    }

    if (node->left == nullptr) {
        transplant(node, node->right);
    } else if (node->right == nullptr) {
        transplant(node, node->left);
    } else {
        Node* successor = minimum(node->right);
        if (successor->parent != node) {
            transplant(successor, successor->right);
            successor->right = node->right;
            successor->right->parent = successor;
        }
        transplant(node, successor);
        successor->left = node->left;
        successor->left->parent = successor;
    }

    delete node;
    --size_;
    return true;
}

const Event* BinarySearchTreeEventSet::find(EventKey key) const {
    Node* current = root_;
    while (current != nullptr) {
        if (key < current->event.key) {
            current = current->left;
        } else if (current->event.key < key) {
            current = current->right;
        } else {
            return &current->event;
        }
    }
    return nullptr;
}

const Event* BinarySearchTreeEventSet::lower_bound(EventKey key) const {
    Node* current = root_;
    Node* candidate = nullptr;
    while (current != nullptr) {
        if (!(current->event.key < key)) {
            candidate = current;
            current = current->left;
        } else {
            current = current->right;
        }
    }
    return candidate == nullptr ? nullptr : &candidate->event;
}

std::vector<Event> BinarySearchTreeEventSet::ordered_events() const {
    std::vector<Event> result;
    result.reserve(size_);
    std::vector<Node*> stack;
    Node* current = root_;

    while (current != nullptr || !stack.empty()) {
        while (current != nullptr) {
            stack.push_back(current);
            current = current->left;
        }
        current = stack.back();
        stack.pop_back();
        result.push_back(current->event);
        current = current->right;
    }
    return result;
}

std::size_t BinarySearchTreeEventSet::size() const noexcept {
    return size_;
}

std::size_t BinarySearchTreeEventSet::height() const {
    if (root_ == nullptr) {
        return 0;
    }

    std::size_t maximum_height = 0;
    std::vector<std::pair<Node*, std::size_t>> stack{{root_, 1}};
    while (!stack.empty()) {
        const auto [node, node_height] = stack.back();
        stack.pop_back();
        maximum_height = std::max(maximum_height, node_height);
        if (node->left != nullptr) {
            stack.emplace_back(node->left, node_height + 1);
        }
        if (node->right != nullptr) {
            stack.emplace_back(node->right, node_height + 1);
        }
    }
    return maximum_height;
}

std::size_t BinarySearchTreeEventSet::estimated_memory_bytes() const noexcept {
    return sizeof(*this) + size_ * sizeof(Node);
}

void BinarySearchTreeEventSet::clear() noexcept {
    // Rotate away left children, then delete nodes along the resulting right spine.
    // This avoids recursion and remains safe for a deliberately degenerate BST.
    Node* current = root_;
    while (current != nullptr) {
        if (current->left != nullptr) {
            Node* promoted = current->left;
            current->left = promoted->right;
            promoted->right = current;
            current = promoted;
        } else {
            Node* next = current->right;
            delete current;
            current = next;
        }
    }
    root_ = nullptr;
    size_ = 0;
}

BinarySearchTreeEventSet::Node* BinarySearchTreeEventSet::minimum(Node* node) noexcept {
    while (node->left != nullptr) {
        node = node->left;
    }
    return node;
}

void BinarySearchTreeEventSet::transplant(Node* old_node, Node* replacement) noexcept {
    if (old_node->parent == nullptr) {
        root_ = replacement;
    } else if (old_node == old_node->parent->left) {
        old_node->parent->left = replacement;
    } else {
        old_node->parent->right = replacement;
    }
    if (replacement != nullptr) {
        replacement->parent = old_node->parent;
    }
}

}  // namespace scheduler
