; RUN: opt -passes=aa-eval -aa-pipeline=evm-aa,basic-aa -print-all-alias-modref-info -disable-output < %s 2>&1 | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

declare void @llvm.evm.memmoveas1as1(ptr addrspace(1) nocapture writeonly, ptr addrspace(1) nocapture readonly, i256, i1 immarg)
declare void @llvm.evm.memcpyas1as2(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(2) noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.evm.memcpyas1as3(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(3) noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.evm.memcpyas1as4(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(4) noalias nocapture readonly, i256, i1 immarg)

; CHECK-LABEL: Function: heap_to_heap:
; CHECK: Both ModRef: Ptr: i256* null <-> call void @llvm.evm.memmoveas1as1
define fastcc void @heap_to_heap(ptr addrspace(1) %dest, ptr addrspace(1) %src) {
  store i256 1, ptr addrspace(1) null, align 1
  call void @llvm.evm.memmoveas1as1(ptr addrspace(1) %dest, ptr addrspace(1) %src, i256 128, i1 false)
  ret void
}

; CHECK-LABEL: Function: calldata_to_heap:
; CHECK: Just Mod: Ptr: i256* null <-> call void @llvm.evm.memcpyas1as2
define fastcc void @calldata_to_heap(ptr addrspace(1) %dest, ptr addrspace(2) %src) {
  store i256 1, ptr addrspace(1) null, align 1
  call void @llvm.evm.memcpyas1as2(ptr addrspace(1) %dest, ptr addrspace(2) %src, i256 128, i1 false)
  ret void
}

; CHECK-LABEL: Function: returndata_to_heap:
; CHECK: Just Mod: Ptr: i256* null <-> call void @llvm.evm.memcpyas1as3
define fastcc void @returndata_to_heap(ptr addrspace(1) %dest, ptr addrspace(3) %src) {
  store i256 1, ptr addrspace(1) null, align 1
  call void @llvm.evm.memcpyas1as3(ptr addrspace(1) %dest, ptr addrspace(3) %src, i256 128, i1 false)
  ret void
}

; CHECK-LABEL: Function: code_to_heap:
; CHECK: Just Mod: Ptr: i256* null <-> call void @llvm.evm.memcpyas1as4
define fastcc void @code_to_heap(ptr addrspace(1) %dest, ptr addrspace(4) %src) {
  store i256 1, ptr addrspace(1) null, align 1
  call void @llvm.evm.memcpyas1as4(ptr addrspace(1) %dest, ptr addrspace(4) %src, i256 128, i1 false)
  ret void
}
