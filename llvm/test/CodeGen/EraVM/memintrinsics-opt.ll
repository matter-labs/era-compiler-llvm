; RUN: opt -O3 -S < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @llvm.memcpy.p0i256.p0i256.i256(i256 addrspace(0)* noalias nocapture writeonly, i256 addrspace(0)* noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p1i256.p1i256.i256(i256 addrspace(1)* noalias nocapture writeonly, i256 addrspace(1)* noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p2i256.p2i256.i256(i256 addrspace(2)* noalias nocapture writeonly, i256 addrspace(2)* noalias nocapture readonly, i256, i1 immarg)

; SimplifyLibCall has this assumption that the size will not exceed i64 datatype.
; CHECK-LABEL: huge-copysize0
define fastcc void @huge-copysize0(i256 addrspace(0)* %dest, i256 addrspace(0)* %src) {
  ; CHECK: tail call void @llvm.memcpy.p0i256.p0i256.i256(i256* align 1 %dest, i256* align 1 %src, i256 81129638414606681695789005144064, i1 false)
  call void @llvm.memcpy.p0i256.p0i256.i256(i256 addrspace(0)* %dest, i256 addrspace(0)* %src, i256 81129638414606681695789005144064, i1 false)
  ret void
}

; CHECK-LABEL: huge-copysize1
define fastcc void @huge-copysize1(i256 addrspace(1)* %dest, i256 addrspace(1)* %src) {
  ; CHECK: tail call void @llvm.memcpy.p1i256.p1i256.i256(i256 addrspace(1)* align 1 %dest, i256 addrspace(1)* align 1 %src, i256 81129638414606681695789005144064, i1 false)
  call void @llvm.memcpy.p1i256.p1i256.i256(i256 addrspace(1)* %dest, i256 addrspace(1)* %src, i256 81129638414606681695789005144064, i1 false)
  ret void
}

; CHECK-LABEL: huge-copysize2
define fastcc void @huge-copysize2(i256 addrspace(2)* %dest, i256 addrspace(2)* %src) {
  ; CHECK: tail call void @llvm.memcpy.p2i256.p2i256.i256(i256 addrspace(2)* align 1 %dest, i256 addrspace(2)* align 1 %src, i256 81129638414606681695789005144064, i1 false)
  call void @llvm.memcpy.p2i256.p2i256.i256(i256 addrspace(2)* %dest, i256 addrspace(2)* %src, i256 81129638414606681695789005144064, i1 false)
  ret void
}
