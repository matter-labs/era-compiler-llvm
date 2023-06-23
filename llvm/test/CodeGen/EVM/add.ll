; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @addrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @addrrr
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ADD [[REG:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = add i256 %rs1, %rs2
  ret i256 %res
}

define i256 @addrri(i256 %rs1) nounwind {
; CHECK-LABEL: @addrri
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[C1:\$[0-9]+]], 18446744073709551616
; CHECK: ADD [[REG:\$[0-9]+]], [[IN1]], [[C1]]

  %res = add i256 %rs1, 18446744073709551616 ; 65-bits
  ret i256 %res
}

define i256 @subrri(i256 %rs1) nounwind {
; CHECK-LABEL: @subrri
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[C1:\$[0-9]+]], 1
; CHECK: ADD [[REG:\$[0-9]+]], [[IN1]], [[C1]]

  %res = sub i256 %rs1, -1
  ret i256 %res
}
