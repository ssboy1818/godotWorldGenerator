#pragma once

#include "Arc.h"
#include "DCEL.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

class BeachLine {
public:
    class Node {
    public:
        Arc &arc() noexcept {
            return m_arc;
        }

        const Arc &arc() const noexcept {
            return m_arc;
        }

    private:
        enum class Color {
            Red,
            Black
        };

        Arc m_arc;
        Color m_color{Color::Red};
        Node *m_parent{nullptr};
        Node *m_left{nullptr};
        Node *m_right{nullptr};
        Node *m_previous{nullptr};
        Node *m_next{nullptr};
        const BeachLine *m_owner{nullptr};

        explicit Node(Arc arc) noexcept
            : m_arc(std::move(arc)) {}

        friend class BeachLine;
    };

    struct SplitResult {
        Node *left{nullptr};
        Node *middle{nullptr};
        Node *right{nullptr};
        CircleEvent *invalidatedEvent{nullptr};
    };

    struct EraseResult {
        Node *previous{nullptr};
        Node *next{nullptr};
        CircleEvent *invalidatedEvent{nullptr};
    };

public:
    BeachLine() noexcept = default;
    ~BeachLine() noexcept {
        clear();
    }

    BeachLine(const BeachLine &) = delete;
    BeachLine &operator=(const BeachLine &) = delete;
    BeachLine(BeachLine &&) = delete;
    BeachLine &operator=(BeachLine &&) = delete;

    [[nodiscard]] bool empty() const noexcept {
        return m_root == nullptr;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return m_size;
    }

    Node *root() noexcept {
        return m_root;
    }

    const Node *root() const noexcept {
        return m_root;
    }

    Node *first() noexcept {
        return m_first;
    }

    const Node *first() const noexcept {
        return m_first;
    }

    Node *last() noexcept {
        return m_last;
    }

    const Node *last() const noexcept {
        return m_last;
    }

    Node *previous(Node *node) const noexcept {
        return node == nullptr ? nullptr : node->m_previous;
    }

    const Node *previous(const Node *node) const noexcept {
        return node == nullptr ? nullptr : node->m_previous;
    }

    Node *next(Node *node) const noexcept {
        return node == nullptr ? nullptr : node->m_next;
    }

    const Node *next(const Node *node) const noexcept {
        return node == nullptr ? nullptr : node->m_next;
    }

    Node *nodeFor(const Arc *arc) const noexcept {
        const auto iterator = m_nodes.find(arc);
        return iterator == m_nodes.end() ? nullptr : iterator->second;
    }

    Node *insertFirst(Arc arc) {
        if (!empty())
            throw std::logic_error("The beach line already contains an arc.");

        auto *node = createNode(std::move(arc));
        node->m_color = Node::Color::Black;
        m_root = node;
        m_first = node;
        m_last = node;
        ++m_size;
        return node;
    }

    Node *insertBefore(Node *position, Arc arc) {
        ensureOwned(position);

        auto *node = createNode(std::move(arc));
        if (position->m_left != nullptr) {
            auto *parent = maximum(position->m_left);
            parent->m_right = node;
            node->m_parent = parent;
        } else {
            position->m_left = node;
            node->m_parent = position;
        }

        node->m_previous = position->m_previous;
        node->m_next = position;
        if (position->m_previous != nullptr)
            position->m_previous->m_next = node;
        else
            m_first = node;
        position->m_previous = node;

        ++m_size;
        fixInsertion(node);
        return node;
    }

    Node *insertAfter(Node *position, Arc arc) {
        ensureOwned(position);

        auto *node = createNode(std::move(arc));
        if (position->m_right != nullptr) {
            auto *parent = minimum(position->m_right);
            parent->m_left = node;
            node->m_parent = parent;
        } else {
            position->m_right = node;
            node->m_parent = position;
        }

        node->m_previous = position;
        node->m_next = position->m_next;
        if (position->m_next != nullptr)
            position->m_next->m_previous = node;
        else
            m_last = node;
        position->m_next = node;

        ++m_size;
        fixInsertion(node);
        return node;
    }

    SplitResult split(Node *node, Arc middleArc) {
        ensureOwned(node);

        Arc rightArc = node->m_arc;
        auto *invalidatedEvent = takePendingEvent(node);
        rightArc.pendingEvent = nullptr;
        node->m_arc.rightEdge = INVALID_ID;

        auto *middle = insertAfter(node, std::move(middleArc));
        auto *right = insertAfter(middle, std::move(rightArc));
        return {node, middle, right, invalidatedEvent};
    }

    EraseResult erase(Node *node) {
        ensureOwned(node);

        EraseResult result{node->m_previous, node->m_next, takePendingEvent(node)};
        if (node->m_previous != nullptr)
            node->m_previous->m_next = node->m_next;
        else
            m_first = node->m_next;

        if (node->m_next != nullptr)
            node->m_next->m_previous = node->m_previous;
        else
            m_last = node->m_previous;

        auto *removed = node;
        auto removedColor = removed->m_color;
        Node *replacement = nullptr;
        Node *replacementParent = nullptr;

        if (node->m_left == nullptr) {
            replacement = node->m_right;
            replacementParent = node->m_parent;
            transplant(node, node->m_right);
        } else if (node->m_right == nullptr) {
            replacement = node->m_left;
            replacementParent = node->m_parent;
            transplant(node, node->m_left);
        } else {
            removed = minimum(node->m_right);
            removedColor = removed->m_color;
            replacement = removed->m_right;

            if (removed->m_parent == node) {
                replacementParent = removed;
                if (replacement != nullptr)
                    replacement->m_parent = removed;
            } else {
                replacementParent = removed->m_parent;
                transplant(removed, removed->m_right);
                removed->m_right = node->m_right;
                removed->m_right->m_parent = removed;
            }

            transplant(node, removed);
            removed->m_left = node->m_left;
            removed->m_left->m_parent = removed;
            removed->m_color = node->m_color;
        }

        node->m_owner = nullptr;
        m_nodes.erase(&node->m_arc);
        delete node;
        --m_size;

        if (removedColor == Node::Color::Black)
            fixDeletion(replacement, replacementParent);

        return result;
    }

    CircleEvent *takePendingEvent(Node *node) {
        ensureOwned(node);
        auto *event = node->m_arc.pendingEvent;
        node->m_arc.pendingEvent = nullptr;
        return event;
    }

    Node *findArcAbove(double x, double sweepLine, const DCEL &dcel) const {
        auto *node = m_root;
        while (node != nullptr) {
            const auto *previousNode = node->m_previous;
            if (previousNode != nullptr) {
                const auto leftBreakpoint = breakpointX(
                    dcel.site(previousNode->m_arc.focus).position,
                    dcel.site(node->m_arc.focus).position,
                    sweepLine);
                if (x < leftBreakpoint) {
                    node = node->m_left;
                    continue;
                }
            }

            const auto *nextNode = node->m_next;
            if (nextNode != nullptr) {
                const auto rightBreakpoint = breakpointX(
                    dcel.site(node->m_arc.focus).position,
                    dcel.site(nextNode->m_arc.focus).position,
                    sweepLine);
                if (x > rightBreakpoint) {
                    node = node->m_right;
                    continue;
                }
            }

            return node;
        }

        return nullptr;
    }

    double rightBreakpoint(const Node *node,
                           double sweepLine,
                           const DCEL &dcel) const {
        ensureOwned(node);
        if (node->m_next == nullptr)
            return std::numeric_limits<double>::infinity();

        return breakpointX(dcel.site(node->m_arc.focus).position,
                           dcel.site(node->m_next->m_arc.focus).position,
                           sweepLine);
    }

    static double breakpointX(const Vector2d &left,
                              const Vector2d &right,
                              double sweepLine) {
        if (std::abs(left.y - right.y) < EPS)
            return (left.x + right.x) / 2.0;
        if (std::abs(left.y - sweepLine) < EPS)
            return left.x;
        if (std::abs(right.y - sweepLine) < EPS)
            return right.x;

        const auto leftDenominator = 2.0 * (left.y - sweepLine);
        const auto rightDenominator = 2.0 * (right.y - sweepLine);
        const auto a = 1.0 / leftDenominator - 1.0 / rightDenominator;
        const auto b = -2.0 * (left.x / leftDenominator - right.x / rightDenominator);
        const auto c = (left.x * left.x + left.y * left.y - sweepLine * sweepLine)
                           / leftDenominator
                     - (right.x * right.x + right.y * right.y - sweepLine * sweepLine)
                           / rightDenominator;
        const auto discriminant = b * b - 4.0 * a * c;
        if (discriminant < -EPS)
            throw std::logic_error("Unable to calculate a beach-line breakpoint.");

        const auto root = std::sqrt(std::max(0.0, discriminant));
        const auto first = (-b + root) / (2.0 * a);
        const auto second = (-b - root) / (2.0 * a);
        return left.y < right.y ? std::max(first, second) : std::min(first, second);
    }

    void clear() noexcept {
        auto *node = m_first;
        while (node != nullptr) {
            auto *nextNode = node->m_next;
            node->m_owner = nullptr;
            delete node;
            node = nextNode;
        }

        m_root = nullptr;
        m_first = nullptr;
        m_last = nullptr;
        m_size = 0;
        m_nodes.clear();
    }

private:
    Node *m_root{nullptr};
    Node *m_first{nullptr};
    Node *m_last{nullptr};
    std::size_t m_size{0};
    std::unordered_map<const Arc *, Node *> m_nodes;

private:
    Node *createNode(Arc arc) {
        auto *node = new Node(std::move(arc));
        node->m_owner = this;
        m_nodes.emplace(&node->m_arc, node);
        return node;
    }

    void ensureOwned(const Node *node) const {
        if (node == nullptr || node->m_owner != this)
            throw std::invalid_argument("The arc does not belong to this beach line.");
    }

    static Node::Color colorOf(const Node *node) noexcept {
        return node == nullptr ? Node::Color::Black : node->m_color;
    }

    static void setColor(Node *node, Node::Color color) noexcept {
        if (node != nullptr)
            node->m_color = color;
    }

    static Node *minimum(Node *node) noexcept {
        while (node->m_left != nullptr)
            node = node->m_left;
        return node;
    }

    static Node *maximum(Node *node) noexcept {
        while (node->m_right != nullptr)
            node = node->m_right;
        return node;
    }

    void rotateLeft(Node *node) noexcept {
        auto *right = node->m_right;
        node->m_right = right->m_left;
        if (right->m_left != nullptr)
            right->m_left->m_parent = node;

        right->m_parent = node->m_parent;
        if (node->m_parent == nullptr)
            m_root = right;
        else if (node == node->m_parent->m_left)
            node->m_parent->m_left = right;
        else
            node->m_parent->m_right = right;

        right->m_left = node;
        node->m_parent = right;
    }

    void rotateRight(Node *node) noexcept {
        auto *left = node->m_left;
        node->m_left = left->m_right;
        if (left->m_right != nullptr)
            left->m_right->m_parent = node;

        left->m_parent = node->m_parent;
        if (node->m_parent == nullptr)
            m_root = left;
        else if (node == node->m_parent->m_right)
            node->m_parent->m_right = left;
        else
            node->m_parent->m_left = left;

        left->m_right = node;
        node->m_parent = left;
    }

    void fixInsertion(Node *node) noexcept {
        while (node != m_root && colorOf(node->m_parent) == Node::Color::Red) {
            auto *parent = node->m_parent;
            auto *grandparent = parent->m_parent;
            if (parent == grandparent->m_left) {
                auto *uncle = grandparent->m_right;
                if (colorOf(uncle) == Node::Color::Red) {
                    setColor(parent, Node::Color::Black);
                    setColor(uncle, Node::Color::Black);
                    setColor(grandparent, Node::Color::Red);
                    node = grandparent;
                } else {
                    if (node == parent->m_right) {
                        node = parent;
                        rotateLeft(node);
                        parent = node->m_parent;
                        grandparent = parent->m_parent;
                    }

                    setColor(parent, Node::Color::Black);
                    setColor(grandparent, Node::Color::Red);
                    rotateRight(grandparent);
                }
            } else {
                auto *uncle = grandparent->m_left;
                if (colorOf(uncle) == Node::Color::Red) {
                    setColor(parent, Node::Color::Black);
                    setColor(uncle, Node::Color::Black);
                    setColor(grandparent, Node::Color::Red);
                    node = grandparent;
                } else {
                    if (node == parent->m_left) {
                        node = parent;
                        rotateRight(node);
                        parent = node->m_parent;
                        grandparent = parent->m_parent;
                    }

                    setColor(parent, Node::Color::Black);
                    setColor(grandparent, Node::Color::Red);
                    rotateLeft(grandparent);
                }
            }
        }

        setColor(m_root, Node::Color::Black);
    }

    void transplant(Node *source, Node *replacement) noexcept {
        if (source->m_parent == nullptr)
            m_root = replacement;
        else if (source == source->m_parent->m_left)
            source->m_parent->m_left = replacement;
        else
            source->m_parent->m_right = replacement;

        if (replacement != nullptr)
            replacement->m_parent = source->m_parent;
    }

    void fixDeletion(Node *node, Node *parent) noexcept {
        while (node != m_root && colorOf(node) == Node::Color::Black) {
            if (node == (parent == nullptr ? nullptr : parent->m_left)) {
                auto *sibling = parent->m_right;
                if (colorOf(sibling) == Node::Color::Red) {
                    setColor(sibling, Node::Color::Black);
                    setColor(parent, Node::Color::Red);
                    rotateLeft(parent);
                    sibling = parent->m_right;
                }

                if (colorOf(sibling == nullptr ? nullptr : sibling->m_left) == Node::Color::Black
                    && colorOf(sibling == nullptr ? nullptr : sibling->m_right)
                        == Node::Color::Black) {
                    setColor(sibling, Node::Color::Red);
                    node = parent;
                    parent = node->m_parent;
                } else {
                    if (colorOf(sibling == nullptr ? nullptr : sibling->m_right)
                        == Node::Color::Black) {
                        setColor(sibling == nullptr ? nullptr : sibling->m_left,
                                 Node::Color::Black);
                        setColor(sibling, Node::Color::Red);
                        rotateRight(sibling);
                        sibling = parent->m_right;
                    }

                    setColor(sibling, colorOf(parent));
                    setColor(parent, Node::Color::Black);
                    setColor(sibling == nullptr ? nullptr : sibling->m_right,
                             Node::Color::Black);
                    rotateLeft(parent);
                    node = m_root;
                    parent = nullptr;
                }
            } else {
                auto *sibling = parent->m_left;
                if (colorOf(sibling) == Node::Color::Red) {
                    setColor(sibling, Node::Color::Black);
                    setColor(parent, Node::Color::Red);
                    rotateRight(parent);
                    sibling = parent->m_left;
                }

                if (colorOf(sibling == nullptr ? nullptr : sibling->m_right)
                        == Node::Color::Black
                    && colorOf(sibling == nullptr ? nullptr : sibling->m_left)
                        == Node::Color::Black) {
                    setColor(sibling, Node::Color::Red);
                    node = parent;
                    parent = node->m_parent;
                } else {
                    if (colorOf(sibling == nullptr ? nullptr : sibling->m_left)
                        == Node::Color::Black) {
                        setColor(sibling == nullptr ? nullptr : sibling->m_right,
                                 Node::Color::Black);
                        setColor(sibling, Node::Color::Red);
                        rotateLeft(sibling);
                        sibling = parent->m_left;
                    }

                    setColor(sibling, colorOf(parent));
                    setColor(parent, Node::Color::Black);
                    setColor(sibling == nullptr ? nullptr : sibling->m_left,
                             Node::Color::Black);
                    rotateRight(parent);
                    node = m_root;
                    parent = nullptr;
                }
            }
        }

        setColor(node, Node::Color::Black);
    }
};
