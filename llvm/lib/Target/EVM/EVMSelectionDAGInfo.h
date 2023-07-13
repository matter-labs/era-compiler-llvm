//=--------- EVMSelectionDAGInfo.h - EVM SelectionDAG Info -*- C++ ---------*-//
//
//
//
//===----------------------------------------------------------------------===//
//
// This file defines the EVM subclass for SelectionDAGTargetInfo.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_EVM_EVMSELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

namespace llvm {

class EVMSelectionDAGInfo final : public SelectionDAGTargetInfo {
public:
  SDValue EmitTargetCodeForMemcpy(SelectionDAG &DAG, const SDLoc &dl,
                                  SDValue Chain, SDValue Op1, SDValue Op2,
                                  SDValue Op3, Align Alignment, bool isVolatile,
                                  bool AlwaysInline,
                                  MachinePointerInfo DstPtrInfo,
                                  MachinePointerInfo SrcPtrInfo) const override;
};

} // end namespace llvm
#endif // LLVM_LIB_TARGET_EVM_EVMSELECTIONDAGINFO_H
