; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @mulrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @mulrrr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: MUL [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = mul i256 %rs1, %rs2
  ret i256 %res
}

define i256 @mulrri(i256 %rs1) nounwind {
; CHECK-LABEL: @mulrri
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[REG1:\$[0-9]+]], 500000
; CHECK: CONST_I256 [[REG2:\$[0-9]+]], 0
; CHECK: SUB [[REG3:\$[0-9]+]], [[REG2]], [[REG1]]
; CHECK: MUL [[TMP:\$[0-9]+]], [[IN1]], [[REG3]]

  %res = mul i256 %rs1, -500000
  ret i256 %res
}
