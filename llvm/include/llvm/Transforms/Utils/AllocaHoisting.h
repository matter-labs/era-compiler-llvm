//===- AllocaHoisting.h - Hoist alloca to entry -----------------*- C++ -*-===//
//
// This file declares a function that hoist all allocas to the entry BB
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_UTILS_ALLOCAHOISTING_H
#define LLVM_TRANSFORMS_UTILS_ALLOCAHOISTING_H


namespace llvm {

class Function;

/// For a given function \p F, moves all alloca to the entry basic block.
/// Return if there are changes.
bool HoistAllocaToEntry(Function& F);

} // end namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_ALLOCAHOISTING_H
