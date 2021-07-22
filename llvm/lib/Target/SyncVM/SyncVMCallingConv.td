//==- SyncVMCallingConv.td - Calling Conventions for SyncVM -*- tablegen -*-==//
//
// This file describes the calling conventions for SyncVM architecture.
//
//===----------------------------------------------------------------------===//



//===----------------------------------------------------------------------===//
// SyncVM Return Value Calling Convention
//===----------------------------------------------------------------------===//
def CC_SYNCVM : CallingConv<[
  CCAssignToReg<[R1, R2, R3, R4, R5, R6]>,
  CCAssignToStack<32, 32>
]>;
