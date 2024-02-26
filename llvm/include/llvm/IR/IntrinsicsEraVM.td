//===-- IntrinsicsTVM.td - Defines EraVM intrinsics --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the EraVM specific intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "eravm" in {

// Calls of an external contract.
def int_eravm_farcall: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], []>;
def int_eravm_delegatecall: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], []>;
def int_eravm_callcode: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], []>;
def int_eravm_staticcall: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], []>;
def int_eravm_mimiccall: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], []>;
def int_eravm_nearcall: Intrinsic<[llvm_i256_ty], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty, llvm_vararg_ty], [Throws]>;

// Read-only access of inaccessible EraVM context data immutable within the current contract execution frame.
def int_eravm_this: Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;
def int_eravm_caller: Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;
def int_eravm_codesource: Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;
def int_eravm_txorigin: Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;
def int_eravm_getu128: Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

// Read-only access of inaccessible EraVM context data mutable within the current contract execution frame.
def int_eravm_meta: Intrinsic<[llvm_i256_ty], [], [IntrInaccessibleMemOnly]>;
def int_eravm_gasleft: Intrinsic<[llvm_i256_ty], [], [IntrInaccessibleMemOnly, IntrNoMerge]>;

// Write access to inaccessible EraVM context data.
def int_eravm_setu128: Intrinsic<[], [llvm_i256_ty], [IntrInaccessibleMemOnly]>;
def int_eravm_inctx: Intrinsic<[], [], [IntrInaccessibleMemOnly]>;
def int_eravm_setpubdataprice: Intrinsic<[], [llvm_i256_ty], [IntrInaccessibleMemOnly]>;
def int_eravm_event: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], [IntrInaccessibleMemOnly]>;
def int_eravm_tol1: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], [IntrInaccessibleMemOnly]>;

// Can have generic side effects, including writing to memory of the current contract.
def int_eravm_precompile: Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrHasSideEffects]>;
def int_eravm_decommit: Intrinsic<[LLVMQualPointerType<llvm_i8_ty, 3>], [llvm_i256_ty, llvm_i256_ty], [IntrHasSideEffects]>;

// Return with error.
def int_eravm_throw: Intrinsic<[], [llvm_i256_ty], [Throws, IntrNoReturn]>;
def int_eravm_return: Intrinsic<[], [llvm_i256_ty], [IntrNoReturn]>;
def int_eravm_revert: Intrinsic<[], [llvm_i256_ty], [IntrNoReturn]>;
def int_eravm_return_ptr: Intrinsic<[], [LLVMQualPointerType<llvm_i8_ty, 3>], [IntrNoReturn]>;
def int_eravm_revert_ptr: Intrinsic<[], [LLVMQualPointerType<llvm_i8_ty, 3>], [IntrNoReturn]>;

// Access to flags.
def int_eravm_iflt  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;
def int_eravm_ifgt  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;
def int_eravm_ifeq  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

// Pointer arithmetics
def int_eravm_ptr_pack : Intrinsic<[LLVMQualPointerType<llvm_i8_ty, 3>], [LLVMQualPointerType<llvm_i8_ty, 3>, llvm_i256_ty], [IntrNoMem]>;
def int_eravm_ptr_shrink : Intrinsic<[LLVMQualPointerType<llvm_i8_ty, 3>], [LLVMQualPointerType<llvm_i8_ty, 3>, llvm_i256_ty], [IntrNoMem]>;
def int_eravm_ptr_add : Intrinsic<[LLVMQualPointerType<llvm_i8_ty, 3>], [LLVMQualPointerType<llvm_i8_ty, 3>, llvm_i256_ty], [IntrNoMem]>;
def int_eravm_ptrtoint : Intrinsic<[llvm_i256_ty], [LLVMQualPointerType<llvm_i8_ty, 3>], [IntrNoMem]>;
}
