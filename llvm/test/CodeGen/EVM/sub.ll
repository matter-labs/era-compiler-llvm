; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @subrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @subrrr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SUB [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = sub i256 %rs1, %rs2
  ret i256 %res
}

define i256 @addrri(i256 %rs1) nounwind {
; CHECK-LABEL: @addrri
; CHECK: CONST_I256 [[C1:\$[0-9]+]], 3
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SUB [[TMP:\$[0-9]+]], [[IN1]], [[C1]]

  %res = add i256 %rs1, -3
  ret i256 %res
}
