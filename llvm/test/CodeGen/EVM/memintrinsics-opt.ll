; RUN: opt -O3 --mtriple=evm -S < %s | FileCheck %s

declare void @llvm.memcpy.p0.p0.i256(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i256, i1 immarg)

define fastcc void @huge-copysize0(ptr %dest, ptr %src) {
; CHECK-LABEL: huge-copysize0
; CHECK: tail call void @llvm.memcpy.p0.p0.i256(ptr align 1 %dest, ptr align 1 %src, i256 81129638414606681695789005144064, i1 false)
  call void @llvm.memcpy.p0.p0.i256(ptr %dest, ptr %src, i256 81129638414606681695789005144064, i1 false)
  ret void
}
