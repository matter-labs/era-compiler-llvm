//===- IntrinsicsTVM.td - Defines SyncVM intrinsics --------*- tablegen -*-===//
//
/// \file
/// Defines all SyncVM intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "syncvm" in {

// Absorbs value to accumulate hash.
def int_syncvm_habs: Intrinsic<[], [llvm_i256_ty], [IntrNoMem, IntrHasSideEffects]>;
// Same as above but alse resets accumulated hash before computing the input.
def int_syncvm_habsr: Intrinsic<[], [llvm_i256_ty], [IntrNoMem, IntrHasSideEffects]>;
// Return has computation results.
def int_syncvm_hout: Intrinsic<[llvm_i256_ty], [], [IntrNoMem, IntrHasSideEffects]>;

// Calls of an external contract
def int_syncvm_farcall: Intrinsic<[], [llvm_i256_ty], []>;
def int_syncvm_delegatecall: Intrinsic<[], [llvm_i256_ty], []>;
def int_syncvm_callcode: Intrinsic<[], [llvm_i256_ty], []>;
def int_syncvm_staticcall: Intrinsic<[], [llvm_i256_ty], []>;
def int_syncvm_farcall_rc: Intrinsic<[llvm_i256_ty], [llvm_i256_ty], []>;
def int_syncvm_delegatecall_rc: Intrinsic<[llvm_i256_ty], [llvm_i256_ty], []>;
def int_syncvm_callcode_rc: Intrinsic<[llvm_i256_ty], [llvm_i256_ty], []>;
def int_syncvm_staticcall_rc: Intrinsic<[llvm_i256_ty], [llvm_i256_ty], []>;

// Get value from the context.
def int_syncvm_getfromcontext: Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrNoMem]>;

// Persistent memory access.
def int_syncvm_setstorage: Intrinsic<[], [llvm_i256_ty], [IntrWriteMem]>;
def int_syncvm_sstore: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], [IntrWriteMem]>;
def int_syncvm_sload: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrReadMem]>;
def int_syncvm_event: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], [IntrHasSideEffects]>;

// Switch context.
def int_syncvm_switchcontext: Intrinsic<[], [], [IntrWriteMem]>;

// Return with error.
def int_syncvm_throw: Intrinsic<[], [], [Throws, IntrNoReturn]>;

// Access to flags.
def int_syncvm_gtflag: Intrinsic<[llvm_i256_ty], [], [IntrHasSideEffects]>;
def int_syncvm_ltflag: Intrinsic<[llvm_i256_ty], [], [IntrHasSideEffects]>;
def int_syncvm_eqflag: Intrinsic<[llvm_i256_ty], [], [IntrHasSideEffects]>;

}
