//==- SyncVMCallingConv.td - Calling Conventions for SyncVM -*- tablegen -*-==//
// This describes the calling conventions for SyncVM architecture.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// SyncVM Return Value Calling Convention
//===----------------------------------------------------------------------===//
def RetCC_SyncVM : CallingConv<[
  // i8 are returned in registers R1, R2, R3, R3
  CCIfType<[v256i1], CCAssignToReg<[R1, R2, R3, R4]>>,
]>;

//===----------------------------------------------------------------------===//
// SyncVM Argument Calling Conventions
//===----------------------------------------------------------------------===//
def CC_SyncVM_AssignStack : CallingConv<[
  // Pass by value if the byval attribute is given
  CCIfByVal<CCPassByVal<2, 2>>,
]>;

