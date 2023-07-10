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

def int_evm_sha3
  : Intrinsic<[llvm_i256_ty], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty],
              [IntrReadMem]>;

// Memory operations.
def int_evm_mstore8
  : Intrinsic<[], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty],
              [IntrWriteMem, IntrHasSideEffects]>;

def int_evm_sload
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrReadMem]>;

def int_evm_sstore
  : Intrinsic<[], [llvm_i256_ty, llvm_i256_ty],
              [IntrWriteMem, IntrHasSideEffects]>;

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

def int_evm_calldataload : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], []>;

def int_evm_calldatacopy
  : Intrinsic<[], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty, llvm_i256_ty],
              [IntrWriteMem]>;

def int_evm_codesize : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_codecopy
  : Intrinsic<[], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty, llvm_i256_ty],
              [IntrWriteMem]>;

// TODO: Ensure the gasprice is fixed while a function execution.
def int_evm_gasprice : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_extcodesize : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrNoMem]>;

def int_evm_extcodecopy
  : Intrinsic<[], [llvm_i256_ty, LLVMPointerType<llvm_i256_ty>, llvm_i256_ty,
                   llvm_i256_ty], [IntrWriteMem]>;

def int_evm_returndatasize : Intrinsic<[llvm_i256_ty], [], []>;

def int_evm_returndatacopy
  : Intrinsic<[], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty, llvm_i256_ty],
              [IntrWriteMem]>;

def int_evm_extcodehash : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrNoMem]>;

def int_evm_blockhash : Intrinsic<[llvm_i256_ty], [llvm_i256_ty], [IntrNoMem]>;

def int_evm_coinbase : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_timestamp : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_number : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_difficulty : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_gaslimit : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_chainid : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

def int_evm_selfbalance : Intrinsic<[llvm_i256_ty], [], []>;

def int_evm_basefee : Intrinsic<[llvm_i256_ty], [], [IntrNoMem]>;

// Logging.
def int_evm_log0
  : Intrinsic<[], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty],
              [IntrWriteMem]>;

def int_evm_log1
  : Intrinsic<[], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty, llvm_i256_ty],
              [IntrWriteMem]>;

def int_evm_log2
  : Intrinsic<[], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty, llvm_i256_ty,
                   llvm_i256_ty],
              [IntrWriteMem]>;

def int_evm_log3
  : Intrinsic<[], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty, llvm_i256_ty,
                   llvm_i256_ty, llvm_i256_ty],
              [IntrWriteMem]>;

def int_evm_log4
  : Intrinsic<[], [LLVMPointerType<llvm_i256_ty>, llvm_i256_ty, llvm_i256_ty,
                   llvm_i256_ty, llvm_i256_ty, llvm_i256_ty],
              [IntrWriteMem]>;

// System calls
def int_evm_create
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, LLVMPointerType<llvm_i256_ty>,
                               llvm_i256_ty], []>;

def int_evm_call
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty,
               LLVMPointerType<llvm_i256_ty>, llvm_i256_ty,
               LLVMPointerType<llvm_i256_ty>, llvm_i256_ty], []>;

def int_evm_callcode
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty, llvm_i256_ty,
               LLVMPointerType<llvm_i256_ty>, llvm_i256_ty,
               LLVMPointerType<llvm_i256_ty>, llvm_i256_ty], []>;

def int_evm_delegatecall
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty, LLVMPointerType<llvm_i256_ty>,
               llvm_i256_ty, LLVMPointerType<llvm_i256_ty>, llvm_i256_ty], []>;

def int_evm_create2
  : Intrinsic<[llvm_i256_ty], [llvm_i256_ty, LLVMPointerType<llvm_i256_ty>,
                               llvm_i256_ty, llvm_i256_ty], []>;

def int_evm_staticcall
  : Intrinsic<[llvm_i256_ty],
              [llvm_i256_ty, llvm_i256_ty, LLVMPointerType<llvm_i256_ty>,
               llvm_i256_ty, LLVMPointerType<llvm_i256_ty>, llvm_i256_ty], []>;


def int_evm_selfdestruct: Intrinsic<[], [llvm_i256_ty], []>;

def int_evm_stop: Intrinsic<[], [], [IntrNoReturn]>;

def int_evm_invalid: Intrinsic<[], [], [IntrNoReturn]>;

// Return with error.
def int_evm_return: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty], [IntrNoReturn]>;

def int_evm_revert: Intrinsic<[], [llvm_i256_ty, llvm_i256_ty], [IntrNoReturn]>;

} // TargetPrefix = "evm"
