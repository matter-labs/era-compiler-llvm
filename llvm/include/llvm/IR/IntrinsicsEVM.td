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

// Ethereum Virtual Machine (EVM) opcodes and corresponding intrinsics can be grouped
// by the runtime effects i.e., how their behavior or outputs are affected by state
// changes during a transaction:
//
//   - 'Readnone'
//     These do not read or write any persistent world-state once execution begins.
//     Their results depend only on provided inputs or fixed environment data
//     (constant during the transaction). They have no lasting side effects and
//     return the same result given the same inputs.
//
//   - 'Volatile (State-Dependent)'
//     These read data from the world state or execution environment that may change
//     within the same transaction (for example, due to prior storage writes, value
//     transfers, or external calls). Re-invoking them after a state mutation can
//     yield a different result. They themselves do not permanently modify state.
//     Such intrinsics have 'IntrInaccessibleMemOnly' attribute, or
//     'IntrReadMem, IntrInaccessibleMemOnly' attributes to model dependency from
//     the State.
//     NOTE: we assume (to be confirmed) that the heap is not part of the State;
//     therefore, it cannot be modified in a volatile manner.
//
//   - 'Side-Effecting (State-Changing)'
//     These opcodes cause persistent changes in world state - writing to storage,
//     emitting logs, creating or destroying contracts, or transferring Ether.
//     Their execution produces lasting side effects recorded on-chain (storage
//     updates, logs, new accounts, etc.).
//     Such intrinsics have empty memory attributes list which means they can
//     arbitrary change the State.

// TODO: Further improvements of intrinsics' attributes, #807

// Arithmetical operations.
def int_evm_div
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem, IntrWillReturn]>;

def int_evm_sdiv
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem, IntrWillReturn]>;

def int_evm_mod
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem, IntrWillReturn]>;

def int_evm_smod
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem, IntrWillReturn]>;

def int_evm_shl
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem, IntrWillReturn]>;

def int_evm_shr
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem, IntrWillReturn]>;

def int_evm_sar
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem, IntrWillReturn]>;

def int_evm_addmod
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty],
              [IntrNoMem, IntrWillReturn]>;

def int_evm_mulmod
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty],
              [IntrNoMem, IntrWillReturn]>;

def int_evm_exp
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem, IntrWillReturn]>;

def int_evm_sha3
  : Intrinsic<[llvm_i256_ty],
              [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty],
              [IntrReadMem,IntrArgMemOnly, IntrWillReturn]>;

def int_evm_signextend
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty], [IntrNoMem, IntrWillReturn]>;

def int_evm_byte
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, llvm_i256_ty],
              [IntrNoMem, IntrWillReturn]>;

// Memory operations.
def int_evm_mstore8 : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty],
                                    [IntrWriteMem, IntrArgMemOnly]>;

// Special getters that must not be relocated.
def int_evm_msize : Intrinsic<[llvm_i256_ty], [], [IntrWillReturn]>;

def int_evm_pc : Intrinsic<[llvm_i256_ty], [], [IntrWillReturn]>;

def int_evm_gas : Intrinsic<[llvm_i256_ty], [], [IntrWillReturn]>;

// Getting values from the context.
def int_evm_address : Intrinsic<[llvm_i256_ty], [], [IntrNoMem, IntrWillReturn]>;

def int_evm_origin : Intrinsic<[llvm_i256_ty], [],
                               [IntrReadMem, IntrInaccessibleMemOnly, IntrWillReturn]>;

def int_evm_caller : Intrinsic<[llvm_i256_ty], [], [IntrNoMem, IntrWillReturn]>;

def int_evm_balance : Intrinsic<[llvm_i256_ty], [llvm_i256_ty],
                                [IntrReadMem, IntrInaccessibleMemOnly, IntrWillReturn]>;

def int_evm_callvalue : Intrinsic<[llvm_i256_ty], [], [IntrNoMem, IntrWillReturn]>;

def int_evm_calldatasize : Intrinsic<[llvm_i256_ty], [], [IntrNoMem, IntrWillReturn]>;

def int_evm_calldataload
  : Intrinsic<[llvm_i256_ty], [LLVMQualPointerType<AS.CALL_DATA>],
              [IntrReadMem, IntrArgMemOnly]>;

def int_evm_codesize : Intrinsic<[llvm_i256_ty], [], [IntrNoMem, IntrWillReturn]>;

def int_evm_gasprice : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly,
                                                      IntrWillReturn]>;

def int_evm_extcodesize
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrReadMem, IntrInaccessibleMemOnly, IntrWillReturn]>;

// This should have 'read of inaccessible' memory along with read/write memory accessed through
// its pointer-typed arguments. Since we can't express that precisely here, we represent
// it as read/write access to arbitrary memory.
def int_evm_extcodecopy
  : Intrinsic<[], [llvm_i256_ty, LLVMQualPointerType<AS.HEAP>,
                  LLVMQualPointerType<AS.CODE>, llvm_i256_ty], [IntrWillReturn]>;

def int_evm_returndatasize
  : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly, IntrWillReturn]>;

def int_evm_extcodehash
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrReadMem, IntrInaccessibleMemOnly, IntrWillReturn]>;

def int_evm_blockhash : Intrinsic<[llvm_i256_ty], [llvm_i256_ty],
                                  [IntrReadMem, IntrInaccessibleMemOnly, IntrWillReturn]>;

def int_evm_blobhash : Intrinsic<[llvm_i256_ty], [llvm_i256_ty],
                                 [IntrReadMem, IntrInaccessibleMemOnly, IntrWillReturn]>;

def int_evm_coinbase : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly,
                                                      IntrWillReturn]>;

def int_evm_timestamp : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly,
                                                       IntrWillReturn]>;

def int_evm_number : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly,
                                                    IntrWillReturn]>;

def int_evm_difficulty : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly,
                                                        IntrWillReturn]>;

def int_evm_gaslimit : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly,
                                                      IntrWillReturn]>;

def int_evm_chainid : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly,
                                                     IntrWillReturn]>;

def int_evm_selfbalance
  : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly, IntrWillReturn]>;

def int_evm_basefee : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly,
                                                     IntrWillReturn]>;

def int_evm_blobbasefee : Intrinsic<[llvm_i256_ty], [], [IntrReadMem, IntrInaccessibleMemOnly,
                                                         IntrWillReturn]>;

// Logging.
// Log intrinsics should have 'read/write of inaccessible' memory along with read memory
// accessed through its pointer-typed arguments. Since we can't express that precisely
// here, we represent it as read/write access to arbitrary memory.
// Note: Attempting to mark them with both IntrReadMem and IntrHasSideEffects won't work,
// because IntrHasSideEffects does not override IntrReadMem. As a result, intrinsics will
// lack the 'HasSideEffects' flag entirely, making them subject to DCE.
def int_evm_log0 : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty],
                             [IntrWillReturn]>;

def int_evm_log1 : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty, llvm_i256_ty],
                             [IntrWillReturn]>;

def int_evm_log2 : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty,
                                 llvm_i256_ty, llvm_i256_ty],
                             [IntrWillReturn]>;

def int_evm_log3 : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty,
                                  llvm_i256_ty, llvm_i256_ty, llvm_i256_ty],
                             [IntrWillReturn]>;

def int_evm_log4 : Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty, llvm_i256_ty,
                                  llvm_i256_ty, llvm_i256_ty, llvm_i256_ty],
                             [IntrWillReturn]>;

// System calls
// The above considerations also apply to xCALL intrinsics.
def int_evm_create
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, LLVMQualPointerType<AS.HEAP>, llvm_i256_ty],
              [IntrWillReturn]>;

def int_evm_call
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], [IntrWillReturn]>;

def int_evm_callcode
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], [IntrWillReturn]>;

def int_evm_delegatecall
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty,
               LLVMQualPointerType<AS.HEAP>, llvm_i256_ty], [IntrWillReturn]>;

def int_evm_create2
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, LLVMQualPointerType<AS.HEAP>,
               llvm_i256_ty, llvm_i256_ty], [IntrWillReturn]>;

def int_evm_staticcall
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty, LLVMQualPointerType<AS.HEAP>,
               llvm_i256_ty, LLVMQualPointerType<AS.HEAP>, llvm_i256_ty],
              [IntrWillReturn]>;


def int_evm_selfdestruct: Intrinsic<[], [llvm_i256_ty], []>;

def int_evm_stop: Intrinsic<[], [], [IntrNoReturn]>;

def int_evm_invalid: Intrinsic<[], [], [IntrNoReturn]>;

// Return with error.
// These should ideally have the IntrReadMem attribute, but doing so causes DSE
// to incorrectly eliminate necessary stores.
def int_evm_return: Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty],
                              [IntrNoReturn]>;

def int_evm_revert: Intrinsic<[], [LLVMQualPointerType<AS.HEAP>, llvm_i256_ty],
                              [IntrNoReturn]>;

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
  [IntrNoMem, IntrWillReturn]
>;

// Returns a code offset of the Yul object with the name
// passed in the metadata.
def int_evm_dataoffset : DefaultAttrsIntrinsic<
  [llvm_i256_ty], [llvm_metadata_ty],
  [IntrNoMem, IntrWillReturn]
>;


// Linking of libraries.

// Inserts a library address placeholder to be replaced with
// a library address at linkage time.
def int_evm_linkersymbol : DefaultAttrsIntrinsic<
  [llvm_i256_ty], [llvm_metadata_ty],
  [IntrNoMem, IntrWillReturn]
>;

// The intrinsic call should not be optimized out, even though
// its result is not used. For this, IntrInaccessibleMemOnly is
// used.
def int_evm_pushdeployaddress : DefaultAttrsIntrinsic<
  [llvm_i256_ty], [], [IntrInaccessibleMemOnly, IntrWillReturn,
                       IntrNoDuplicate]
>;

// Immutables support.

// Loads immutable value. Should be used only in runtime code.
def int_evm_loadimmutable : DefaultAttrsIntrinsic<
  [llvm_i256_ty], [llvm_metadata_ty],
  [IntrNoMem, IntrWillReturn]
>;
} // TargetPrefix = "evm"
