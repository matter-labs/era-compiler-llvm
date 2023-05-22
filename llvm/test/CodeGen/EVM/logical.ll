; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @andrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @andrrr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: AND [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = and i256 %rs1, %rs2
  ret i256 %res
}

define i256 @orrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @orrrr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: OR [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = or i256 %rs1, %rs2
  ret i256 %res
}

define i256 @xorrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @xorrrr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
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
