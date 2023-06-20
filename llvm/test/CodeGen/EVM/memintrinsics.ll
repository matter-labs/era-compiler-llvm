; RUN: llc -O3 --mtriple=evm < %s | FileCheck %s

declare void @llvm.memcpy.p0i256.p0i256.i256(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i256, i1 immarg)

define fastcc void @huge-copysize0(ptr %dest, ptr %src) {
; CHECK-LABEL: huge-copysize0
; CHECK: CONST_I256 ${{.*}}, 2535301200456458802993406410752

  call void @llvm.memcpy.p0i256.p0i256.i256(ptr %dest, ptr %src, i256 81129638414606681695789005144064, i1 false)
  ret void
}

define fastcc void @huge-copysize1(ptr %dest, ptr %src) {
; CHECK-LABEL: huge-copysize1
; CHECK-LABEL: .BB1_1:
; CHECK: CONST_I256 ${{.*}}, 2535301200456458802993406410752
; CHECK: CONST_I256 ${{.*}}, 81129638414606681695789005144064
; CHECK: CONST_I256 ${{.*}}, 452312848583266388373324160190187140051835877600158453279131187530910662655

  call void @llvm.memcpy.p0i256.p0i256.i256(ptr %dest, ptr %src, i256 81129638414606681695789005144065, i1 false)
  ret void
}

define fastcc void @normal-known-size(ptr %dest, ptr %src) {
; CHECK-LABEL: normal-known-size
; CHECK: CONST_I256 ${{.*}}, 32

  call void @llvm.memcpy.p0i256.p0i256.i256(ptr %dest, ptr %src, i256 1024, i1 false)
  ret void
}

define fastcc void @normal-known-size-2(ptr %dest, ptr %src) {
; CHECK-LABEL: normal-known-size-2
; CHECK-LABEL: .BB3_1:
; CHECK: CONST_I256 ${{.*}}, 33
; CHECK: CONST_I256 ${{.*}}, 26959946667150639794667015087019630673637144422540572481103610249215
; CHECK: CONST_I256 ${{.*}}, 115792089210356248756420345214020892766250353992003419616917011526809519390720

  call void @llvm.memcpy.p0i256.p0i256.i256(ptr %dest, ptr %src, i256 1060, i1 false)
  ret void
}
