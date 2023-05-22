; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @udivrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @udivrrr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: DIV [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = udiv i256 %rs1, %rs2
  ret i256 %res
}

define i256 @sdivrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @sdivrrr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SDIV [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = sdiv i256 %rs1, %rs2
  ret i256 %res
}

define i256 @sdivrri(i256 %rs1) nounwind {
; CHECK-LABEL: @sdivrri
; CHECK: CONST_I256 [[TMP:\$[0-9]+]], 0

  %res = sdiv i256 %rs1, 0
  ret i256 %res
}
