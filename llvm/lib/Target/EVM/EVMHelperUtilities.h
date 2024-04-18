#ifndef LLVM_LIB_TARGET_EVM_EVMHELPERUTILITIES_H
#define LLVM_LIB_TARGET_EVM_EVMHELPERUTILITIES_H

#include "llvm/ADT/iterator_range.h"
#include <algorithm>
#include <cassert>
#include <list>
#include <optional>
#include <set>

namespace llvm {

/// Concatenate the contents of a container onto a vector
template <class T, class U>
std::vector<T> &operator+=(std::vector<T> &_a, U &_b) {
  for (auto const &i : _b)
    _a.push_back(T(i));
  return _a;
}

template <class T, class U>
std::vector<T> &operator+=(std::vector<T> &_a, U &&_b) {
  std::move(_b.begin(), _b.end(), std::back_inserter(_a));
  return _a;
}

/// Concatenate two vectors of elements.
template <class T>
inline std::vector<T> operator+(std::vector<T> const &_a,
                                std::vector<T> const &_b) {
  std::vector<T> ret(_a);
  ret += _b;
  return ret;
}

/// Concatenate two vectors of elements, moving them.
template <class T>
inline std::vector<T> operator+(std::vector<T> &&_a, std::vector<T> &&_b) {
  std::vector<T> ret(std::move(_a));
  assert(&_a != &_b);
  ret += std::move(_b);
  return ret;
}

namespace EVMUtils {

template <class T, class V> bool contains(const T &t, const V &v) {
  return std::end(t) != std::find(std::begin(t), std::end(t), v);
}

template <typename Range, typename Value>
std::optional<size_t> findOffset(Range &&range, Value &&value) {
  auto begin = std::begin(range);
  auto end = std::end(range);
  auto it = std::find(begin, end, std::forward<Value>(value));
  if (it == end)
    return std::nullopt;
  return std::distance(begin, it);
}

template <typename T>
iterator_range<typename T::const_iterator> take_last(const T &t, size_t N) {
  auto It = t.end();
  assert(N <= t.size());
  std::advance(It, -N);
  return make_range(It, t.end());
}

template <typename Range>
auto take_last(Range &&r,
               size_t N) -> iterator_range<decltype(Range::begin())> {
  auto It = r.end();
  std::advance(It, -N);
  return make_range(It, r.end());
}

template <typename T>
iterator_range<typename T::const_iterator> drop_first(const T &t, size_t N) {
  auto It = t.begin();
  assert(N <= t.size());
  std::advance(It, N);
  return make_range(It, t.end());
}

template <typename Range>
auto drop_first(Range &&r, size_t N) -> iterator_range<decltype(r.begin())> {
  auto It = r.begin();
  std::advance(It, N);
  return make_range(It, r.end());
}

template <typename T>
iterator_range<typename T::const_reverse_iterator> get_reverse(const T &t) {
  return llvm::make_range(t.rbegin(), t.rend());
}

template <typename T, typename Value>
void push_if_noexist(T &t, Value &&value) {
  if (t.end() == std::find(t.begin(), t.end(), value))
    t.emplace_back(value);
}

/// Generic breadth first search.
///
/// Note that V needs to be a comparable value type or a pointer.
///
/// Example: Gather all (recursive) children in a graph starting at (and
/// including) ``root``:
///
/// Node const* root = ...;
/// std::set<Node const*> allNodes = BreadthFirstSearch<Node
/// const*>{{root}}.run([](Node const* _node, auto&& _addChild) {
///   // Potentially process ``_node``.
///   for (Node const& _child: _node->children())
///     // Potentially filter the children to be visited.
///     _addChild(&_child);
/// }).visited;
///
template <typename V> struct BreadthFirstSearch {
  /// Runs the breadth first search. The verticesToTraverse member of the struct
  /// needs to be initialized.
  /// @param _forEachChild is a callable of the form [...](V const& _node,
  /// auto&& _addChild) { ... } that is called for each visited node and is
  /// supposed to call _addChild(childNode) for every child node of _node.
  template <typename ForEachChild>
  BreadthFirstSearch &run(ForEachChild &&_forEachChild) {
    while (!verticesToTraverse.empty()) {
      V v = std::move(verticesToTraverse.front());
      verticesToTraverse.pop_front();

      if (!visited.insert(v).second)
        continue;

      _forEachChild(v, [this](V _vertex) {
        verticesToTraverse.emplace_back(std::move(_vertex));
      });
    }
    return *this;
  }

  void abort() { verticesToTraverse.clear(); }

  std::list<V> verticesToTraverse;
  std::set<V> visited{};
};

} // namespace EVMUtils
} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMHELPERUTILITIES_H
