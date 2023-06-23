; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @sexti1(i1 %rs1) nounwind {
; CHECK-LABEL: @sexti1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[MASK:\$[0-9]+]], 1
; CHECK: AND [[TMP:\$[0-9]+]], [[IN1]], [[MASK]]
; CHECK: CONST_I256 [[ZERO:\$[0-9]+]], 0
; CHECK: SUB [[RES:\$[0-9]+]], [[ZERO]], [[TMP]]

  %res = sext i1 %rs1 to i256
  ret i256 %res
}

define i256 @sexti8(i8 %rs1) nounwind {
; CHECK-LABEL: @sexti8
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 0
; CHECK: SIGNEXTEND [[TMP:\$[0-9]+]], [[EXT]], [[IN1]]

  %res = sext i8 %rs1 to i256
  ret i256 %res
}

define i256 @sexti16(i16 %rs1) nounwind {
; CHECK-LABEL: @sexti16
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 1
; CHECK: SIGNEXTEND [[TMP:\$[0-9]+]], [[EXT]], [[IN1]]

  %res = sext i16 %rs1 to i256
  ret i256 %res
}

define i256 @sexti32(i32 %rs1) nounwind {
; CHECK-LABEL: @sexti32
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 3
; CHECK: SIGNEXTEND [[TMP:\$[0-9]+]], [[EXT]], [[IN1]]

  %res = sext i32 %rs1 to i256
  ret i256 %res
}

define i256 @sexti64(i64 %rs1) nounwind {
; CHECK-LABEL: @sexti64
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 7
; CHECK: SIGNEXTEND [[TMP:\$[0-9]+]], [[EXT]], [[IN1]]

  %res = sext i64 %rs1 to i256
  ret i256 %res
}

define i256 @sexti128(i128 %rs1) nounwind {
; CHECK-LABEL: @sexti128
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 15
; CHECK: SIGNEXTEND [[TMP:\$[0-9]+]], [[EXT]], [[IN1]]

  %res = sext i128 %rs1 to i256
  ret i256 %res
}

; Check that 'sext' also gets lowered for types not declared in MVT.
define i256 @sexti40(i40 %rs1) nounwind {
; CHECK-LABEL: @sexti40
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[C1:\$[0-9]+]], 216
; CHECK: SHL [[TMP1:\$[0-9]+]], [[IN1]], [[C1]]
; CHECK: SAR [[TMP2:\$[0-9]]], [[TMP1]], [[C1]]

  %res = sext i40 %rs1 to i256
  ret i256 %res
}
