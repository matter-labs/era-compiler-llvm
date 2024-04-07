//===---- EraVMNearCallLowering.cpp - SDAGBuilder's nearcall code ---------===//
//
// This file includes support code use by SelectionDAGBuilder when lowering a
// nearcall sequence in SelectionDAG IR.
//
//===----------------------------------------------------------------------===//

#include "SelectionDAGBuilder.h"
#include "llvm/IR/IntrinsicsEraVM.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-nearcall-lowering"

void SelectionDAGBuilder::LowerEraVMNearCall(
    const CallBase &CB, const BasicBlock *EHPadBB /*= nullptr*/) {
  assert(CB.getIntrinsicID() == Intrinsic::eravm_nearcall);

  auto *CalleeFunction = dyn_cast<Function>(CB.getOperand(0));

  // Support pre-opaque pointers convetion to pass a pointer to a function
  // as i256*
  if (!CalleeFunction) {
    auto *CalleeBitcast = dyn_cast<BitCastInst>(CB.getOperand(0));
    auto *CalleeBitcastOp = dyn_cast<BitCastOperator>(CB.getOperand(0));
    assert(CalleeBitcast || CalleeBitcastOp && "Expected bitcast");

    CalleeFunction = [CalleeBitcast, CalleeBitcastOp]() {
      if (CalleeBitcast)
        return dyn_cast<Function>(CalleeBitcast->getOperand(0));
      return dyn_cast<Function>(CalleeBitcastOp->getOperand(0));
    }();
  }

  SDValue Callee = getValue(CalleeFunction);

  FunctionType *FTy = CalleeFunction->getFunctionType();
  Type *RetTy = CalleeFunction->getType();

  TargetLowering::ArgListTy Args;
  Args.reserve(CalleeFunction->arg_size());

  TargetLowering::CallLoweringInfo CLI(DAG);
  CLI.EraVMAbiData = getValue(CB.getOperand(1));

  for (auto II = std::next(std::next(CB.arg_begin())), IE = CB.arg_end();
       II != IE; ++II) {
    TargetLowering::ArgListEntry Entry;
    const Value *V = *II;

    // Skip empty types
    if (V->getType()->isEmptyTy())
      continue;

    SDValue ArgNode = getValue(V);
    Entry.Node = ArgNode;
    Entry.Ty = V->getType();

    Entry.setAttributes(&CB, II - CB.arg_begin());

    Args.push_back(Entry);
  }

  CLI.setDebugLoc(getCurSDLoc())
      .setChain(getRoot())
      .setCallee(RetTy, FTy, Callee, std::move(Args), CB)
      .setTailCall(false)
      .setConvergent(CB.isConvergent())
      .setIsPreallocated(
          CB.countOperandBundlesOfType(LLVMContext::OB_preallocated) != 0);

  std::pair<SDValue, SDValue> Result = lowerInvokable(CLI, EHPadBB);

  if (Result.first.getNode()) {
    Result.first = lowerRangeToAssertZExt(DAG, CB, Result.first);
    setValue(&CB, Result.first);
  }
}
