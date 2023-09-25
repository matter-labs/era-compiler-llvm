//==- EraVMCallingConv.td - Calling Conventions for EraVM -*- tablegen ---*-==//
//
// This file describes the calling conventions for EraVM architecture.
//
//===----------------------------------------------------------------------===//



//===----------------------------------------------------------------------===//
// EraVM Return Value Calling Convention
//===----------------------------------------------------------------------===//
def RetCC_ERAVM : CallingConv<[
  CCIfType<[i1, i8, i16, i32, i64, i128], CCPromoteToType<i256>>,

  // return values are stored inside R1
  CCAssignToReg<[R1]>,

  // if, there are any other returned value (in the future), store on stack
  CCAssignToStack<32, 1>,
]>;

//===----------------------------------------------------------------------===//
// EraVM Argument Calling Conventions
//===----------------------------------------------------------------------===//
def CC_ERAVM : CallingConv<[
  // promote all integer types to i256
  CCIfType<[i1, i8, i16, i32, i64, i128], CCPromoteToType<i256>>,

  // EraVM doesn't have callee saved registers at the moment,
  // so use all register but R15 to pass arguments.
  // R15 is reserved for abi data parameter.
  CCAssignToReg<[R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14]>,
  CCAssignToStack<32, 1>,
]>;
