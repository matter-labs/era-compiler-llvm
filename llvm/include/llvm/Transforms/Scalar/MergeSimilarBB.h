//===----- MergeSimilarBB.h - Merge similar basic blocks --------*- C++ -*-===//
/// \file
/// This file provides the interface for the pass responsible for merging
/// similar basic blocks.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_MERGESIMILARBB_H
#define LLVM_TRANSFORMS_SCALAR_MERGESIMILARBB_H

#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

/// A pass to merge similar basic blocks.
///
/// The pass identifies isomorphic basic blocks by hashing its content, then it
/// attempt to merge BBs with the same hash.
class MergeSimilarBB : public PassInfoMixin<MergeSimilarBB> {
public:
  /// Run the pass over the function.
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif
