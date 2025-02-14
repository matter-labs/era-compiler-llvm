; RUN: llc --evm-keep-registers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i256 @andrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @andrrr
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: AND [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = and i256 %rs1, %rs2
  ret i256 %res
}

define i256 @orrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @orrrr
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: OR [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = or i256 %rs1, %rs2
  ret i256 %res
}

define i256 @xorrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @xorrrr
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: XOR [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = xor i256 %rs1, %rs2
  ret i256 %res
}

define i256 @notrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @notrrr
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: NOT [[TMP:\$[0-9]+]], [[IN1]]

  %res = xor i256 %rs1, -1
  ret i256 %res
}
