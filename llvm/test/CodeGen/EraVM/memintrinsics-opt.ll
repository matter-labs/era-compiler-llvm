; RUN: opt -opaque-pointers -O3 -S < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @llvm.memcpy.p0.p0.i256(ptr addrspace(0) noalias nocapture writeonly, ptr addrspace(0) noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(1) noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p2.p2.i256(ptr addrspace(2) noalias nocapture writeonly, ptr addrspace(2) noalias nocapture readonly, i256, i1 immarg)

; SimplifyLibCall has this assumption that the size will not exceed i64 datatype.
; CHECK-LABEL: huge-copysize0
define fastcc void @huge-copysize0(i256 addrspace(0)* %dest, i256 addrspace(0)* %src) {
  ; CHECK: tail call void @llvm.memcpy.p0.p0.i256(ptr align 1 %dest, ptr align 1 %src, i256 81129638414606681695789005144064, i1 false)
  call void @llvm.memcpy.p0.p0.i256(ptr addrspace(0) %dest, ptr addrspace(0) %src, i256 81129638414606681695789005144064, i1 false)
  ret void
}

; CHECK-LABEL: huge-copysize1
define fastcc void @huge-copysize1(i256 addrspace(1)* %dest, i256 addrspace(1)* %src) {
  ; CHECK: tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) align 1 %dest, ptr  addrspace(1) align 1 %src, i256 81129638414606681695789005144064, i1 false)
  call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) %dest, ptr addrspace(1) %src, i256 81129638414606681695789005144064, i1 false)
  ret void
}

; CHECK-LABEL: huge-copysize2
define fastcc void @huge-copysize2(i256 addrspace(2)* %dest, i256 addrspace(2)* %src) {
  ; CHECK: tail call void @llvm.memcpy.p2.p2.i256(ptr addrspace(2) align 1 %dest, ptr  addrspace(2) align 1 %src, i256 81129638414606681695789005144064, i1 false)
  call void @llvm.memcpy.p2.p2.i256(ptr addrspace(2) %dest, ptr addrspace(2) %src, i256 81129638414606681695789005144064, i1 false)
  ret void
}
