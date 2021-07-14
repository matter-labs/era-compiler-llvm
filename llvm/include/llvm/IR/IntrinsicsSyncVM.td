//===- IntrinsicsTVM.td - Defines SyncVM intrinsics --------*- tablegen -*-===//
//
/// \file
/// Defines all SyncVM intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "syncvm" in {

// Absorbs value to accumulate hash.
def int_syncvm_habs: Intrinsic<[], [llvm_i256_ty], []>;
// Same as above but alse resets accumulated hash before computing the input.
def int_syncvm_habsr: Intrinsic<[], [llvm_i256_ty], []>;
// Return has computation results.
def int_syncvm_hout: Intrinsic<[llvm_i256_ty], [], []>;

// Call an external contract.
def int_syncvm_farcall: Intrinsic<[], [llvm_i256_ty], []>;

// Get value from the context.
def int_syncvm_getfromcontext: Intrinsic<[llvm_i256_ty], [llvm_i256_ty], []>;

// Persistent memory access.
def int_syncvm_setstorage: Intrinsic<[], [llvm_i256_ty], []>;
def int_syncvm_sstore: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], []>;
def int_syncvm_sload: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], []>;
def int_syncvm_event: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], []>;

// Switch context.
def int_syncvm_switchcontext: Intrinsic<[], [], []>;

// Return with error.
def int_syncvm_throw: Intrinsic<[], [], [Throws, IntrNoReturn]>;

// Access to flags.
def int_syncvm_gtflag: Intrinsic<[llvm_i256_ty], [], []>;
def int_syncvm_ltflag: Intrinsic<[llvm_i256_ty], [], []>;
def int_syncvm_eqflag: Intrinsic<[llvm_i256_ty], [], []>;

}
