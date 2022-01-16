//==- SyncVMCallingConv.td - Calling Conventions for SyncVM -*- tablegen -*-==//
//
// This file describes the calling conventions for SyncVM architecture.
//
//===----------------------------------------------------------------------===//



//===----------------------------------------------------------------------===//
// SyncVM Return Value Calling Convention
//===----------------------------------------------------------------------===//
def RetCC_SYNCVM : CallingConv<[
  CCIfType<[i1, i8, i16, i32, i64, i128], CCPromoteToType<i256>>,

  // return values are stored inside R1
  CCAssignToReg<[R1]>,

  // if, there are any other returned value (in the future), store on stack
  CCAssignToStack<32, 1>,
]>;

//===----------------------------------------------------------------------===//
// SyncVM Argument Calling Conventions
//===----------------------------------------------------------------------===//
def CC_SYNCVM : CallingConv<[
  // promote all integer types to i256
  CCIfType<[i8, i16, i32, i64, i128], CCPromoteToType<i256>>,

  CCAssignToReg<[R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15]>,
  CCAssignToStack<32, 1>,
]>;
