//===----- EVMHelperUtilities.h - CFG for stackification --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines different utility templates. In particular, for a partial
// emulation of the C++ range-V3 library functionality; BFS graph traversal;
// the overload pattern support.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMHELPERUTILITIES_H
#define LLVM_LIB_TARGET_EVM_EVMHELPERUTILITIES_H

#include "llvm/ADT/iterator_range.h"
#include <algorithm>
#include <cassert>
#include <list>
#include <numeric>
#include <optional>
#include <set>

namespace llvm {

template <class... Ts> struct Overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overload(Ts...) -> Overload<Ts...>;

template <class T> static inline SmallVector<T> iota17(T begin, T end) {
  SmallVector<T> R(end - begin);
  std::iota(R.begin(), R.end(), begin);
  return R;
}

/// Concatenate the contents of a container into a vector
template <class T, class U>
std::vector<T> &operator+=(std::vector<T> &lhs, U &rhs) {
  for (auto const &elm : rhs)
    lhs.push_back(T(elm));
  return lhs;
}

template <class T, class U>
std::vector<T> &operator+=(std::vector<T> &lhs, U &&rhs) {
  std::move(rhs.begin(), rhs.end(), std::back_inserter(lhs));
  return lhs;
}

/// Concatenate two vectors of elements.
template <class T>
inline std::vector<T> operator+(std::vector<T> const &lhs,
                                std::vector<T> const &rhs) {
  std::vector<T> result(lhs);
  result += rhs;
  return result;
}

/// Concatenate two vectors of elements, moving them.
template <class T>
inline std::vector<T> operator+(std::vector<T> &&lhs, std::vector<T> &&rhs) {
  std::vector<T> result(std::move(lhs));
  assert(&lhs != &rhs);
  result += std::move(rhs);
  return result;
}

namespace EVMUtils {

template <class T, class V> bool contains(const T &t, const V &v) {
  return std::end(t) != std::find(std::begin(t), std::end(t), v);
}

template <typename T, typename V>
std::optional<size_t> findOffset(T &&t, V &&v) {
  auto begin = std::begin(t);
  auto end = std::end(t);
  auto it = std::find(begin, end, std::forward<V>(v));
  if (it == end)
    return std::nullopt;
  return std::distance(begin, it);
}

template <typename T>
auto take_last(T &&t, size_t N) -> iterator_range<decltype(t.begin())> {
  auto it = t.end();
  std::advance(it, -N);
  return make_range(it, t.end());
}

template <typename T>
auto drop_first(T &&t, size_t N) -> iterator_range<decltype(t.begin())> {
  auto it = t.begin();
  std::advance(it, N);
  return make_range(it, t.end());
}

template <typename T>
iterator_range<typename T::const_reverse_iterator> get_reverse(const T &t) {
  return llvm::make_range(t.rbegin(), t.rend());
}

// Returns a pointer to the entry of map \p m at the key \p k,
// if there is one, and nullptr otherwise.
template <typename M, typename K>
decltype(auto) valueOrNullptr(M &&m, K const &k) {
  auto it = m.find(k);
  return (it == m.end()) ? nullptr : &it->second;
}

template <typename R> auto to_vector(R &&r) {
  std::vector<typename decltype(r.begin())::value_type> v;
  v.assign(r.begin(), r.end());
  return v;
}

/// RAII utility class whose destructor calls a given function.
class ScopeGuard {
public:
  explicit ScopeGuard(std::function<void(void)> Func) : Func(std::move(Func)) {}
  ~ScopeGuard() { Func(); }

private:
  std::function<void(void)> Func;
};

template <typename T, typename V> void emplace_back_unique(T &t, V &&v) {
  if (t.end() == std::find(t.begin(), t.end(), v))
    t.emplace_back(v);
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
  BreadthFirstSearch &run(ForEachChild &&forEachChild) {
    while (!verticesToTraverse.empty()) {
      V v = std::move(verticesToTraverse.front());
      verticesToTraverse.pop_front();

      if (!visited.insert(v).second)
        continue;

      forEachChild(v, [this](V vertex) {
        verticesToTraverse.emplace_back(std::move(vertex));
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
