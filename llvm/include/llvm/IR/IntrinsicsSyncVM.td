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

}
