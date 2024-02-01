//===---- IntrinsicsEVM.td - Defines EVM intrinsics --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines all EVM intrinsics.
//
//===----------------------------------------------------------------------===//

// This must be in sync with the declaration of address spaces in EVM.h
def AS {
  int STACK = 0;
  int HEAP = 1;
  int CALL_DATA = 2;
  int RETURN_DATA = 3;
  int CODE = 4;
  int STORAGE = 5;
  int TSTORAGE = 6;
}

let TargetPrefix = "evm" in {

// Arithmetical operations.
def int_evm_div
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

def int_evm_sdiv
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

def int_evm_mod
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

def int_evm_smod
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

def int_evm_shl
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

def int_evm_shr
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

def int_evm_sar
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

def int_evm_addmod
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty],
              [IntrNoMem]>;

def int_evm_mulmod
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty],
              [IntrNoMem]>;

def int_evm_exp
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

def int_evm_sha3
  : Intrinsic<[llvm_i256_ty],
              [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], [IntrReadMem]>;

def int_evm_signextend
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem]>;

def int_evm_byte
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty],
              [IntrNoMem]>;

// Memory operations.
def int_evm_mstore8
  : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], [IntrWriteMem]>;

// TODO: CPR-1555.
//       Review system intrinsics attributes taking into account EraVM.
def int_evm_msize : Intrinsic<[llvm_i256_ty], [], []>;

def int_evm_pc : Intrinsic<[llvm_i256_ty], [], []>;

def int_evm_gas : Intrinsic<[llvm_i256_ty], [], []>;


// Getting values from the context.
def int_evm_address : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_origin : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_caller : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_balance : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], []>;

def int_evm_callvalue : Intrinsic<[llvm_i256_ty], [], []>;

def int_evm_calldatasize : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_calldataload
  : Intrinsic<[llvm_i256_ty], [LLVMQualPointerType<AS.CALL_DATA>], []>;

def int_evm_codesize : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

// TODO: Ensure the gasprice is fixed while a function execution.
def int_evm_gasprice : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_extcodesize
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrNoMem]>;

def int_evm_extcodecopy
  : Intrinsic<[], [llvm_i256_ty, LLVMQualPointerType<AS.HEAP>,
                   LLVMQualPointerType<AS.CODE>, llvm_i256_ty], [IntrWriteMem]>;

def int_evm_returndatasize : Intrinsic<[llvm_i256_ty], [], []>;

def int_evm_extcodehash
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrNoMem]>;

def int_evm_blockhash : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrNoMem]>;

def int_evm_blobhash : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrNoMem]>;

def int_evm_coinbase : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_timestamp : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_number : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_difficulty : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_gaslimit : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_chainid : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_selfbalance : Intrinsic<[llvm_i256_ty], [], []>;

def int_evm_basefee : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_blobbasefee : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

// Logging.
def int_evm_log0
  : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], []>;

def int_evm_log1
  : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty, llvm_i256_ty], []>;

def int_evm_log2
  : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty, llvm_i256_ty,
                   llvm_i256_ty], []>;

def int_evm_log3
  : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty, llvm_i256_ty,
                   llvm_i256_ty, llvm_i256_ty], []>;

def int_evm_log4
  : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty, llvm_i256_ty,
                   llvm_i256_ty, llvm_i256_ty, llvm_i256_ty], []>;

// System calls
def int_evm_create
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], []>;

def int_evm_call
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], []>;

def int_evm_callcode
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], []>;

def int_evm_delegatecall
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], []>;

def int_evm_create2
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, LLVMQualPointerType<AS.HEAP>,
                               llvm_i256_ty, llvm_i256_ty], []>;

def int_evm_staticcall
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty, LLVMQualPointerType<AS.HEAP>,
               llvm_i256_ty, LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], []>;


def int_evm_selfdestruct: Intrinsic<[], [llvm_i256_ty], []>;

def int_evm_stop: Intrinsic<[], [], [IntrNoReturn]>;

def int_evm_invalid: Intrinsic<[], [], [IntrNoReturn]>;

// Return with error.
def int_evm_return: Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty],
                              [IntrNoReturn]>;

def int_evm_revert: Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty],
                              [IntrNoReturn]>;

// Stack manipulation.
def int_evm_pop: Intrinsic<[], [llvm_i256_ty], [IntrNoReturn]>;

def int_evm_memmoveas1as1
  : Intrinsic<[],
              [LLVMQualPointerType<AS.HEAP>, LLVMQualPointerType<AS.HEAP>,
               llvm_i256_ty, llvm_i1_ty],
              [IntrArgMemOnly, IntrWillReturn, IntrNoFree,
               IntrNoCallback,
               NoCapture<ArgIndex<0>>, NoCapture<ArgIndex<1>>,
               WriteOnly<ArgIndex<0>>, ReadOnly<ArgIndex<1>>,
               ImmArg<ArgIndex<3>>]>;

def int_evm_memcpyas1as2
  : Intrinsic<[],
              [LLVMQualPointerType<AS.HEAP>, LLVMQualPointerType<AS.CALL_DATA>,
               llvm_i256_ty, llvm_i1_ty],
              [IntrArgMemOnly, IntrWillReturn, IntrNoFree,
               IntrNoCallback,
               NoCapture<ArgIndex<0>>, NoCapture<ArgIndex<1>>,
               NoAlias<ArgIndex<0>>, NoAlias<ArgIndex<1>>,
               WriteOnly<ArgIndex<0>>, ReadOnly<ArgIndex<1>>,
               ImmArg<ArgIndex<3>>]>;

def int_evm_memcpyas1as3
  : Intrinsic<[],
              [LLVMQualPointerType<AS.HEAP>, LLVMQualPointerType<AS.RETURN_DATA>,
               llvm_i256_ty, llvm_i1_ty],
              [IntrArgMemOnly, IntrWillReturn, IntrNoFree,
               IntrNoCallback,
               NoCapture<ArgIndex<0>>, NoCapture<ArgIndex<1>>,
               NoAlias<ArgIndex<0>>, NoAlias<ArgIndex<1>>,
               WriteOnly<ArgIndex<0>>, ReadOnly<ArgIndex<1>>,
               ImmArg<ArgIndex<3>>]>;

def int_evm_memcpyas1as4
  : Intrinsic<[],
              [LLVMQualPointerType<AS.HEAP>, LLVMQualPointerType<AS.CODE>,
               llvm_i256_ty, llvm_i1_ty],
              [IntrArgMemOnly, IntrWillReturn, IntrNoFree,
               IntrNoCallback,
               NoCapture<ArgIndex<0>>, NoCapture<ArgIndex<1>>,
               NoAlias<ArgIndex<0>>, NoAlias<ArgIndex<1>>,
               WriteOnly<ArgIndex<0>>, ReadOnly<ArgIndex<1>>,
               ImmArg<ArgIndex<3>>]>;

// The following intrinsics are used for linking purposes.

// Returns a code size of the Yul object with the name
// passed in the metadata.
def int_evm_datasize : DefaultAttrsIntrinsic<
  [llvm_i256_ty], [llvm_metadata_ty],
  [IntrNoMem, IntrSpeculatable]
>;

// Returns a code offset of the Yul object with the name
// passed in the metadata.
def int_evm_dataoffset : DefaultAttrsIntrinsic<
  [llvm_i256_ty], [llvm_metadata_ty],
  [IntrNoMem, IntrSpeculatable]
>;

} // TargetPrefix = "evm"
