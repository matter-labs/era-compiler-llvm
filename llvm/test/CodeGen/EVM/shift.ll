; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @shl(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @shl
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SHL [[RES:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = shl i256 %rs1, %rs2
  ret i256 %res
}

define i256 @shr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @shr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SHR [[RES:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = lshr i256 %rs1, %rs2
  ret i256 %res
}

define i256 @sar(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @sar
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SAR [[RES:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = ashr i256 %rs1, %rs2
  ret i256 %res
}
