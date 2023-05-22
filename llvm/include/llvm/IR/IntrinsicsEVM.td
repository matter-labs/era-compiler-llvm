//===---- IntrinsicsEVM.td - Defines EVM intrinsics --------*- tablegen -*-===//
//
// Defines all EVM intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "evm" in {

// Arithmetical operations.
def int_evm_addmod
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty],
              [IntrNoMem]>;

def int_evm_mulmod
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty],
              [IntrNoMem]>;

def int_evm_exp
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;
}
