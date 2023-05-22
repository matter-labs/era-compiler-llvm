; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @umodrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @umodrrr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: MOD [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = urem i256 %rs1, %rs2
  ret i256 %res
}

define i256 @smodrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @smodrrr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SMOD [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = srem i256 %rs1, %rs2
  ret i256 %res
}

define i256 @smodrri(i256 %rs1) nounwind {
; CHECK-LABEL: @smodrri
; CHECK: CONST_I256 [[TMP:\$[0-9]+]], 0

  %res = srem i256 %rs1, 0
  ret i256 %res
}
