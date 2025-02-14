; RUN: llc --evm-keep-registers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i256 @umodrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @umodrrr
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: MOD [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = urem i256 %rs1, %rs2
  ret i256 %res
}

define i256 @smodrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @smodrrr
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
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
