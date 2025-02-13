; RUN: opt -O3 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

declare void @llvm.memcpy.p1.p2.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(2) noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(3) noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(4) noalias nocapture readonly, i256, i1 immarg)

define fastcc void @calldata_to_heap() {
; CHECK-LABEL: calldata_to_heap
; CHECK: llvm.memcpy.p1.p2.i256

  call void @llvm.memcpy.p1.p2.i256(ptr addrspace(1) null, ptr addrspace(2) null, i256 4, i1 false)
  ret void
}

define fastcc void @returndata_to_heap() {
; CHECK-LABEL: returndata_to_heap
; CHECK: llvm.memcpy.p1.p3.i256

  call void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) null, ptr addrspace(3) null, i256 4, i1 false)
  ret void
}

define fastcc void @code_to_heap() {
; CHECK-LABEL: code_to_heap
; CHECK: llvm.memcpy.p1.p4.i256

  call void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) null, ptr addrspace(4) null, i256 4, i1 false)
  ret void
}
