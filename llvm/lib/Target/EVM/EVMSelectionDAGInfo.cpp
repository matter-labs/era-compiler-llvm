//===------------ EVMSelectionDAGInfo.cpp - EVM SelectionDAG Info ---------===//
//
//
//
//===----------------------------------------------------------------------===//
//
// This file implements the EVMSelectionDAGInfo class.
//
//===----------------------------------------------------------------------===//
#include "EVM.h"
#include "EVMTargetMachine.h"
using namespace llvm;

#define DEBUG_TYPE "evm-selectiondag-info"

SDValue EVMSelectionDAGInfo::EmitTargetCodeForMemcpy(
    SelectionDAG &DAG, const SDLoc &DL, SDValue Chain, SDValue Dst, SDValue Src,
    SDValue Size, Align Alignment, bool IsVolatile, bool AlwaysInline,
    MachinePointerInfo DstPtrInfo, MachinePointerInfo SrcPtrInfo) const {

  assert(DstPtrInfo.getAddrSpace() == EVMAS::AS_HEAP &&
         "heap is expected as the memcpy destination");

  unsigned ISDNode = 0;
  switch (SrcPtrInfo.getAddrSpace()) {
  default:
    llvm_unreachable("Unexpected source address space of memcpy");
    break;
  case EVMAS::AS_CALL_DATA:
    ISDNode = EVMISD::MEMCPY_CALL_DATA;
    break;
  case EVMAS::AS_RETURN_DATA:
    ISDNode = EVMISD::MEMCPY_RETURN_DATA;
    break;
  case EVMAS::AS_CODE:
    ISDNode = EVMISD::MEMCPY_CODE;
    break;
  }

  return DAG.getNode(ISDNode, DL, MVT::Other, {Chain, Dst, Src, Size});
}
