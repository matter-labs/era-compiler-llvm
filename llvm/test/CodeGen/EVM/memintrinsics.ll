; RUN: llc -O3 --evm-keep-registers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

declare void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(1) noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p1.p2.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(2) noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(3) noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(4) noalias nocapture readonly, i256, i1 immarg)

declare void @llvm.memmove.p1.p1.i256(ptr addrspace(1) nocapture writeonly, ptr addrspace(1) nocapture readonly, i256, i1 immarg)
declare void @llvm.memmove.p1.p2.i256(ptr addrspace(1) nocapture writeonly, ptr addrspace(2) nocapture readonly, i256, i1 immarg)
declare void @llvm.memmove.p1.p3.i256(ptr addrspace(1) nocapture writeonly, ptr addrspace(3) nocapture readonly, i256, i1 immarg)
declare void @llvm.memmove.p1.p4.i256(ptr addrspace(1) nocapture writeonly, ptr addrspace(4) nocapture readonly, i256, i1 immarg)
declare void @llvm.memmove.p1.p1.i8(ptr addrspace(1) nocapture writeonly, ptr addrspace(1) nocapture readonly, i8, i1 immarg)

define fastcc void @memmove-imm8(ptr addrspace(1) %dest, ptr addrspace(1) %src) {
; CHECK-LABEL: memmove-imm8
; CHECK: MCOPY

  call void @llvm.memmove.p1.p1.i8(ptr addrspace(1) %dest, ptr addrspace(1) %src, i8 77, i1 false)
  ret void
}

define fastcc void @memmove-imm8-arg(ptr addrspace(1) %dest, ptr addrspace(1) %src, i8 %size) {
; CHECK-LABEL: memmove-imm8-arg
; CHECK: MCOPY

  call void @llvm.memmove.p1.p1.i8(ptr addrspace(1) %dest, ptr addrspace(1) %src, i8 %size, i1 false)
  ret void
}

define fastcc void @huge-copysize0(ptr addrspace(1) %dest, ptr addrspace(1) %src) {
; CHECK-LABEL: huge-copysize0
; CHECK: MCOPY

  call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) %dest, ptr addrspace(1) %src, i256 81129638414606681695789005144064, i1 false)
  ret void
}

define fastcc void @huge-copysize1(ptr addrspace(1) %dest, ptr addrspace(1) %src) {
; CHECK-LABEL: huge-copysize1
; CHECK: MCOPY

  call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) %dest, ptr addrspace(1) %src, i256 81129638414606681695789005144065, i1 false)
  ret void
}

define fastcc void @huge-movesize1(ptr addrspace(1) %dest, ptr addrspace(1) %src) {
; CHECK-LABEL: huge-movesize1
; CHECK: MCOPY

  call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) %dest, ptr addrspace(1) %src, i256 81129638414606681695789005144065, i1 false)
  ret void
}

define fastcc void @normal-known-size(ptr addrspace(1) %dest, ptr addrspace(1) %src) {
; CHECK-LABEL: normal-known-size
; CHECK: MCOPY

  call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) %dest, ptr addrspace(1) %src, i256 1024, i1 false)
  ret void
}

define fastcc void @normal-known-size-2(ptr addrspace(1) %dest, ptr addrspace(1) %src) {
; CHECK-LABEL: normal-known-size-2
; CHECK: MCOPY

  call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) %dest, ptr addrspace(1) %src, i256 1060, i1 false)
  ret void
}

define fastcc void @heap_to_heap(ptr addrspace(1) %dest, ptr addrspace(1) %src, i256 %len) {
; CHECK-LABEL: heap_to_heap
; CHECK: MCOPY

  call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) %dest, ptr addrspace(1) %src, i256 %len, i1 false)
  ret void
}

define fastcc void @calldata_to_heap(ptr addrspace(1) %dest, ptr addrspace(2) %src, i256 %len) {
; CHECK-LABEL: calldata_to_heap
; CHECK: CALLDATACOPY

  call void @llvm.memcpy.p1.p2.i256(ptr addrspace(1) %dest, ptr addrspace(2) %src, i256 %len, i1 false)
  ret void
}

define fastcc void @move_calldata_to_heap(ptr addrspace(1) %dest, ptr addrspace(2) %src, i256 %len) {
; CHECK-LABEL: move_calldata_to_heap
; CHECK: CALLDATACOPY

  call void @llvm.memcpy.p1.p2.i256(ptr addrspace(1) %dest, ptr addrspace(2) %src, i256 %len, i1 false)
  ret void
}

define fastcc void @calldata_to_heap_csize(ptr addrspace(1) %dest, ptr addrspace(2) %src) {
; CHECK-LABEL: calldata_to_heap_csize
; CHECK: CALLDATACOPY

  call void @llvm.memcpy.p1.p2.i256(ptr addrspace(1) %dest, ptr addrspace(2) %src, i256 42, i1 false)
  ret void
}

define fastcc void @returndata_to_heap(ptr addrspace(1) %dest, ptr addrspace(3) %src, i256 %len) {
; CHECK-LABEL: returndata_to_heap
; CHECK: RETURNDATACOPY

  call void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) %dest, ptr addrspace(3) %src, i256 %len, i1 false)
  ret void
}

define fastcc void @move_returndata_to_heap(ptr addrspace(1) %dest, ptr addrspace(3) %src, i256 %len) {
; CHECK-LABEL: move_returndata_to_heap
; CHECK: RETURNDATACOPY

  call void @llvm.memmove.p1.p3.i256(ptr addrspace(1) %dest, ptr addrspace(3) %src, i256 %len, i1 false)
  ret void
}

define fastcc void @code_to_heap(ptr addrspace(1) %dest, ptr addrspace(4) %src, i256 %len) {
; CHECK-LABEL: code_to_heap
; CHECK: CODECOPY

  call void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) %dest, ptr addrspace(4) %src, i256 %len, i1 false)
  ret void
}

define fastcc void @move_code_to_heap(ptr addrspace(1) %dest, ptr addrspace(4) %src, i256 %len) {
; CHECK-LABEL: move_code_to_heap
; CHECK: CODECOPY

  call void @llvm.memmove.p1.p4.i256(ptr addrspace(1) %dest, ptr addrspace(4) %src, i256 %len, i1 false)
  ret void
}
