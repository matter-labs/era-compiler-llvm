; RUN: llc -O3 < %s

; This test case is reduced with llvm-reduce.
; Before the fix, we were hitting an assert during
; stackification in EVMStackModel::getStackSlot. This
; was caused by not updating the live interval of ADD
; operand during rematerialization in EVMSingleUseExpression pass.

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

; Function Attrs: nounwind willreturn memory(argmem: read)
declare i256 @llvm.evm.sha3(ptr addrspace(1), i256) #0

; Function Attrs: nounwind willreturn
declare i256 @llvm.evm.call(i256, i256, i256, ptr addrspace(1), i256, ptr addrspace(1), i256) #1

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memmove.p1.p1.i256(ptr addrspace(1) nocapture writeonly, ptr addrspace(1) nocapture readonly, i256, i1 immarg) #2

define void @__entry() {
entry:
  %abi_decode_bytest_bytest_bytest_bytes_call406 = call fastcc { i256, i256, i256, i256 } @abi_decode_bytest_bytest_bytest_bytes()
  %abi_decode_bytest_bytest_bytest_bytes_call406.fca.0.extract = extractvalue { i256, i256, i256, i256 } %abi_decode_bytest_bytest_bytest_bytes_call406, 0
  %abi_decode_bytest_bytest_bytest_bytes_call406.fca.2.extract = extractvalue { i256, i256, i256, i256 } %abi_decode_bytest_bytest_bytest_bytes_call406, 2
  %addition_result419 = add i256 %abi_decode_bytest_bytest_bytest_bytes_call406.fca.2.extract, 1
  %keccak256_input_offset_pointer = inttoptr i256 %addition_result419 to ptr addrspace(1)
  %keccak256 = tail call i256 @llvm.evm.sha3(ptr addrspace(1) %keccak256_input_offset_pointer, i256 0)
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) null, ptr addrspace(1) %keccak256_input_offset_pointer, i256 0, i1 false)
  %call_input_offset_pointer829 = inttoptr i256 %abi_decode_bytest_bytest_bytest_bytes_call406.fca.0.extract to ptr addrspace(1)
  %call831 = tail call i256 @llvm.evm.call(i256 0, i256 0, i256 0, ptr addrspace(1) %call_input_offset_pointer829, i256 0, ptr addrspace(1) null, i256 0)
  ret void
}

declare fastcc { i256, i256, i256, i256 } @abi_decode_bytest_bytest_bytest_bytes()

attributes #0 = { nounwind willreturn memory(argmem: read) }
attributes #1 = { nounwind willreturn }
attributes #2 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
