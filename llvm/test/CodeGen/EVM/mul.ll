; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @mulrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @mulrrr
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 0
; CHECK: MUL [[IN3:\$[0-9]+]], [[IN2]], [[IN1]]

  %res = mul i256 %rs1, %rs2
  ret i256 %res
}
