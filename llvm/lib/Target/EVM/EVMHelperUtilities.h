//===----------- EVMHelperUtilities.h - Helper utilities --------*- C++ -*-===//
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

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/iterator_range.h"
#include <deque>
#include <numeric>
#include <optional>

namespace llvm {

class MachineInstr;

template <class... Ts> struct Overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overload(Ts...) -> Overload<Ts...>;

namespace EVMUtils {
bool callWillReturn(const MachineInstr *Call);

/// Return a vector, containing sequentially increasing values from \p Begin
/// to \p End.
template <class T> static inline SmallVector<T> iota(T Begin, T End) {
  SmallVector<T> R(End - Begin);
  std::iota(R.begin(), R.end(), Begin);
  return R;
}

/// Return the number of hops from the beginning of the \p RangeOrContainer
/// to the \p Item. If no \p Item is found in the \p RangeOrContainer,
/// std::nullopt is returned.
template <typename T, typename V>
std::optional<size_t> offset(T &&RangeOrContainer, V &&Item) {
  auto It = find(RangeOrContainer, Item);
  return (It == adl_end(RangeOrContainer))
             ? std::nullopt
             : std::optional(std::distance(adl_begin(RangeOrContainer), It));
}

/// Return a range covering  the last N elements of \p RangeOrContainer.
template <typename T> auto take_back(T &&RangeOrContainer, size_t N = 1) {
  return make_range(std::prev(adl_end(RangeOrContainer), N),
                    adl_end(RangeOrContainer));
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

  std::deque<V> verticesToTraverse;
  DenseSet<V> visited{};
};

} // namespace EVMUtils
} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMHELPERUTILITIES_H
