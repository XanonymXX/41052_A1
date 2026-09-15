#include "red_black_tree_event_set.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace scheduler {

RedBlackTreeEventSet::RedBlackTreeEventSet() {
    nil_ = new Node(Event{}, Color::black, nullptr);
    nil_->parent = nil_;
    nil_->left = nil_;
    nil_->right = nil_;
    root_ = nil_;
}

RedBlackTreeEventSet::~RedBlackTreeEventSet() {
    clear();
    delete nil_;
}

bool RedBlackTreeEventSet::insert(Event event) {
    Node* parent = nil_;
    Node* current = root_;
    while (current != nil_) {
        parent = current;
        if (event.key < current->event.key) {
            current = current->left;
        } else if (current->event.key < event.key) {
            current = current->right;
        } else {
            return false;
        }
    }

    auto* node = new Node(std::move(event), Color::red, nil_);
    node->parent = parent;
    if (parent == nil_) {
        root_ = node;
    } else if (node->event.key < parent->event.key) {
        parent->left = node;
    } else {
        parent->right = node;
    }

    ++size_;
    insert_fixup(node);
    return true;
}

bool RedBlackTreeEventSet::erase(EventKey key) {
    Node* target = find_node(key);
    if (target == nil_) {
        return false;
    }

    Node* removed_or_moved = target;
    Color original_color = removed_or_moved->color;
    Node* fixup_node = nil_;

    if (target->left == nil_) {
        fixup_node = target->right;
        transplant(target, target->right);
    } else if (target->right == nil_) {
        fixup_node = target->left;
        transplant(target, target->left);
    } else {
        removed_or_moved = minimum(target->right);
        original_color = removed_or_moved->color;
        fixup_node = removed_or_moved->right;
        if (removed_or_moved->parent == target) {
            fixup_node->parent = removed_or_moved;
        } else {
            transplant(removed_or_moved, removed_or_moved->right);
            removed_or_moved->right = target->right;
            removed_or_moved->right->parent = removed_or_moved;
        }
        transplant(target, removed_or_moved);
        removed_or_moved->left = target->left;
        removed_or_moved->left->parent = removed_or_moved;
        removed_or_moved->color = target->color;
    }

    delete target;
    --size_;
    if (original_color == Color::black) {
        erase_fixup(fixup_node);
    }
    nil_->parent = nil_;
    return true;
}

const Event* RedBlackTreeEventSet::find(EventKey key) const {
    Node* node = find_node(key);
    return node == nil_ ? nullptr : &node->event;
}

const Event* RedBlackTreeEventSet::lower_bound(EventKey key) const {
    Node* current = root_;
    Node* candidate = nil_;
    while (current != nil_) {
        if (!(current->event.key < key)) {
            candidate = current;
            current = current->left;
        } else {
            current = current->right;
        }
    }
    return candidate == nil_ ? nullptr : &candidate->event;
}

std::vector<Event> RedBlackTreeEventSet::ordered_events() const {
    std::vector<Event> result;
    result.reserve(size_);
    std::vector<Node*> stack;
    Node* current = root_;
    while (current != nil_ || !stack.empty()) {
        while (current != nil_) {
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

std::size_t RedBlackTreeEventSet::size() const noexcept {
    return size_;
}

std::size_t RedBlackTreeEventSet::height() const {
    if (root_ == nil_) {
        return 0;
    }

    std::size_t maximum_height = 0;
    std::vector<std::pair<Node*, std::size_t>> stack{{root_, 1}};
    while (!stack.empty()) {
        const auto [node, node_height] = stack.back();
        stack.pop_back();
        maximum_height = std::max(maximum_height, node_height);
        if (node->left != nil_) {
            stack.emplace_back(node->left, node_height + 1);
        }
        if (node->right != nil_) {
            stack.emplace_back(node->right, node_height + 1);
        }
    }
    return maximum_height;
}

std::size_t RedBlackTreeEventSet::estimated_memory_bytes() const noexcept {
    return sizeof(*this) + (size_ + 1) * sizeof(Node);
}

void RedBlackTreeEventSet::clear() noexcept {
    Node* current = root_;
    while (current != nil_) {
        if (current->left != nil_) {
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
    root_ = nil_;
    size_ = 0;
    nil_->parent = nil_;
}

bool RedBlackTreeEventSet::validate_invariants() const noexcept {
    if (nil_ == nullptr || nil_->color != Color::black || root_ == nullptr) {
        return false;
    }
    if (root_ == nil_) {
        return size_ == 0;
    }
    if (root_->parent != nil_ || root_->color != Color::black) {
        return false;
    }

    std::size_t visited = 0;
    const int black_height = validate_subtree(root_, nullptr, nullptr, visited);
    return black_height > 0 && visited == size_;
}

RedBlackTreeEventSet::Node* RedBlackTreeEventSet::find_node(EventKey key) const noexcept {
    Node* current = root_;
    while (current != nil_) {
        if (key < current->event.key) {
            current = current->left;
        } else if (current->event.key < key) {
            current = current->right;
        } else {
            return current;
        }
    }
    return nil_;
}

RedBlackTreeEventSet::Node* RedBlackTreeEventSet::minimum(Node* node) const noexcept {
    while (node->left != nil_) {
        node = node->left;
    }
    return node;
}

void RedBlackTreeEventSet::left_rotate(Node* node) noexcept {
    Node* pivot = node->right;
    node->right = pivot->left;
    if (pivot->left != nil_) {
        pivot->left->parent = node;
    }
    pivot->parent = node->parent;
    if (node->parent == nil_) {
        root_ = pivot;
    } else if (node == node->parent->left) {
        node->parent->left = pivot;
    } else {
        node->parent->right = pivot;
    }
    pivot->left = node;
    node->parent = pivot;
}

void RedBlackTreeEventSet::right_rotate(Node* node) noexcept {
    Node* pivot = node->left;
    node->left = pivot->right;
    if (pivot->right != nil_) {
        pivot->right->parent = node;
    }
    pivot->parent = node->parent;
    if (node->parent == nil_) {
        root_ = pivot;
    } else if (node == node->parent->right) {
        node->parent->right = pivot;
    } else {
        node->parent->left = pivot;
    }
    pivot->right = node;
    node->parent = pivot;
}

void RedBlackTreeEventSet::insert_fixup(Node* node) noexcept {
    while (node->parent->color == Color::red) {
        if (node->parent == node->parent->parent->left) {
            Node* uncle = node->parent->parent->right;
            if (uncle->color == Color::red) {
                node->parent->color = Color::black;
                uncle->color = Color::black;
                node->parent->parent->color = Color::red;
                node = node->parent->parent;
            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    left_rotate(node);
                }
                node->parent->color = Color::black;
                node->parent->parent->color = Color::red;
                right_rotate(node->parent->parent);
            }
        } else {
            Node* uncle = node->parent->parent->left;
            if (uncle->color == Color::red) {
                node->parent->color = Color::black;
                uncle->color = Color::black;
                node->parent->parent->color = Color::red;
                node = node->parent->parent;
            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    right_rotate(node);
                }
                node->parent->color = Color::black;
                node->parent->parent->color = Color::red;
                left_rotate(node->parent->parent);
            }
        }
    }
    root_->color = Color::black;
}

void RedBlackTreeEventSet::erase_fixup(Node* node) noexcept {
    while (node != root_ && node->color == Color::black) {
        if (node == node->parent->left) {
            Node* sibling = node->parent->right;
            if (sibling->color == Color::red) {
                sibling->color = Color::black;
                node->parent->color = Color::red;
                left_rotate(node->parent);
                sibling = node->parent->right;
            }
            if (sibling->left->color == Color::black && sibling->right->color == Color::black) {
                sibling->color = Color::red;
                node = node->parent;
            } else {
                if (sibling->right->color == Color::black) {
                    sibling->left->color = Color::black;
                    sibling->color = Color::red;
                    right_rotate(sibling);
                    sibling = node->parent->right;
                }
                sibling->color = node->parent->color;
                node->parent->color = Color::black;
                sibling->right->color = Color::black;
                left_rotate(node->parent);
                node = root_;
            }
        } else {
            Node* sibling = node->parent->left;
            if (sibling->color == Color::red) {
                sibling->color = Color::black;
                node->parent->color = Color::red;
                right_rotate(node->parent);
                sibling = node->parent->left;
            }
            if (sibling->right->color == Color::black && sibling->left->color == Color::black) {
                sibling->color = Color::red;
                node = node->parent;
            } else {
                if (sibling->left->color == Color::black) {
                    sibling->right->color = Color::black;
                    sibling->color = Color::red;
                    left_rotate(sibling);
                    sibling = node->parent->left;
                }
                sibling->color = node->parent->color;
                node->parent->color = Color::black;
                sibling->left->color = Color::black;
                right_rotate(node->parent);
                node = root_;
            }
        }
    }
    node->color = Color::black;
}

void RedBlackTreeEventSet::transplant(Node* old_node, Node* replacement) noexcept {
    if (old_node->parent == nil_) {
        root_ = replacement;
    } else if (old_node == old_node->parent->left) {
        old_node->parent->left = replacement;
    } else {
        old_node->parent->right = replacement;
    }
    replacement->parent = old_node->parent;
}

int RedBlackTreeEventSet::validate_subtree(
    const Node* node,
    const EventKey* minimum_key,
    const EventKey* maximum_key,
    std::size_t& visited) const noexcept {
    if (node == nil_) {
        return 1;
    }
    if (node == nullptr || node->left == nullptr || node->right == nullptr || node->parent == nullptr) {
        return -1;
    }
    if ((minimum_key != nullptr && !( *minimum_key < node->event.key)) ||
        (maximum_key != nullptr && !(node->event.key < *maximum_key))) {
        return -1;
    }
    if ((node->left != nil_ && node->left->parent != node) ||
        (node->right != nil_ && node->right->parent != node)) {
        return -1;
    }
    if (node->color == Color::red &&
        (node->left->color == Color::red || node->right->color == Color::red)) {
        return -1;
    }

    ++visited;
    const int left_black_height = validate_subtree(node->left, minimum_key, &node->event.key, visited);
    const int right_black_height = validate_subtree(node->right, &node->event.key, maximum_key, visited);
    if (left_black_height < 0 || right_black_height < 0 || left_black_height != right_black_height) {
        return -1;
    }
    return left_black_height + (node->color == Color::black ? 1 : 0);
}

}  // namespace scheduler
