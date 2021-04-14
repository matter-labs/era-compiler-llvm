; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: habs
define void @habs(i256 %in) nounwind {
; CHECK: habs r1
  call void @llvm.syncvm.habs(i256 %in)
  ret void
}

; CHECK-LABEL: habsr
define void @habsr(i256 %in) nounwind {
; CHECK: habsr r1
  call void @llvm.syncvm.habsr(i256 %in)
  ret void
}

; CHECK-LABEL: hout
define i256 @hout() nounwind {
; CHECK: hout r1
  %1 = call i256 @llvm.syncvm.hout()
  ret i256 %1
}

; CHECK-LABEL: memcpy00
define void @memcpy00(i256* align 256 %dest, i256* align 256 %src, i256 %size) {
  call void @llvm.memcpy.p0i256.p0i256.i256(i256* align 256 %dest, i256* align 256 %src, i256 %size, i1 false)
  ret void
}

; CHECK-LABEL: memcpy02
define void @memcpy02(i256* align 256 %dest, i256 addrspace(2)* align 256 %src, i256 %size) {
  call void @llvm.memcpy.p0i256.p2i256.i256(i256* align 256 %dest, i256 addrspace(2)* align 256 %src, i256 %size, i1 false)
  ret void
}

; CHECK-LABEL: memcpy30
define void @memcpy30(i256 addrspace(3)* align 256 %dest, i256* align 256 %src, i256 %size) {
  call void @llvm.memcpy.p3i256.p0i256.i256(i256 addrspace(3)* align 256 %dest, i256* align 256 %src, i256 %size, i1 false)
  ret void
}

; CHECK-LABEL: memmov00
define void @memmov00(i256* align 256 %dest, i256* align 256 %src, i256 %size) {
  call void @llvm.memmov.p0i256.p0i256.i256(i256* align 256 %dest, i256* align 256 %src, i256 %size, i1 false)
  ret void
}

; TODO: memset require val to be i8. Needs to be fixed.
; CHECK-LABEL: memset0
define void @memset0(i256* %dest, i8 %val, i256 %size) {
  call void @llvm.memset.p0i256.i256(i256* %dest, i8 %val, i256 %size, i1 false)
  ret void
}

declare void @llvm.syncvm.habs(i256 %in)
declare void @llvm.syncvm.habsr(i256 %in)
declare i256 @llvm.syncvm.hout()
declare void @llvm.memcpy.p0i256.p0i256.i256(i256*, i256*, i256, i1)
declare void @llvm.memcpy.p0i256.p2i256.i256(i256*, i256 addrspace(2)*, i256, i1)
declare void @llvm.memcpy.p3i256.p0i256.i256(i256 addrspace(3)*, i256*, i256, i1)
declare void @llvm.memmov.p0i256.p0i256.i256(i256*, i256*, i256, i1)
declare void @llvm.memset.p0i256.i256(i256*, i8, i256, i1)
