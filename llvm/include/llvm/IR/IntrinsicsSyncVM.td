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
def int_syncvm_call: Intrinsic<[llvm_ptr_ty], [llvm_vararg_ty], []>;

// Number of cycles remained.
def int_syncvm_cyclesremain: Intrinsic<[llvm_i256_ty], [], []>;

// Persistent memory access.
def int_syncvm_sstore: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty], []>;
def int_syncvm_sload: Intrinsic<[llvm_i256_ty], [llvm_i256_ty], []>;

}
