//===- IntrinsicsTVM.td - Defines SyncVM intrinsics --------*- tablegen -*-===//
//
/// \file
/// Defines all SyncVM intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "syncvm" in {

// Calls of an external contract.
def int_syncvm_farcall: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], []>;
def int_syncvm_delegatecall: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], []>;
def int_syncvm_callcode: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], []>;
def int_syncvm_staticcall: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], []>;
def int_syncvm_mimiccall: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], []>;
def int_syncvm_nearcall: Intrinsic<[llvm_i256_ty], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty, llvm_vararg_ty], [Throws]>;

// Get value from the context.
def int_syncvm_this: Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;
def int_syncvm_caller: Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;
def int_syncvm_codesource: Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;
def int_syncvm_meta: Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;
def int_syncvm_txorigin: Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;
def int_syncvm_ergsleft: Intrinsic<[llvm_i256_ty], [], []>;
def int_syncvm_getu128: Intrinsic<[llvm_i256_ty], [], []>;
def int_syncvm_setu128: Intrinsic<[], [llvm_i256_ty], []>;

// Persistent memory access.
def int_syncvm_sstore: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty], [IntrWriteMem, Throws]>;
def int_syncvm_sload: Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrReadMem]>;
def int_syncvm_event: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], [IntrHasSideEffects]>;
def int_syncvm_tol1: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], [IntrHasSideEffects]>;
def int_syncvm_precompile: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrHasSideEffects]>;

// Return with error.
def int_syncvm_throw: Intrinsic<[], [llvm_i256_ty], [Throws, IntrNoReturn]>;
def int_syncvm_return: Intrinsic<[], [llvm_i256_ty], [IntrNoReturn]>;
def int_syncvm_revert: Intrinsic<[], [llvm_i256_ty], [IntrNoReturn]>;

// Access to flags.
def int_syncvm_iflt  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;
def int_syncvm_ifgt  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;
def int_syncvm_ifeq  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

// Pointer arithmetics
def int_syncvm_ptr_pack : Intrinsic<[LLVMQualPointerType<llvm_i8_ty, 3>], [llvm_any_ty, llvm_i256_ty], [IntrNoMem]>;
def int_syncvm_ptr_add : Intrinsic<[LLVMQualPointerType<llvm_i8_ty, 3>], [LLVMQualPointerType<llvm_i8_ty, 3>, llvm_i256_ty], [IntrNoMem]>;
}
