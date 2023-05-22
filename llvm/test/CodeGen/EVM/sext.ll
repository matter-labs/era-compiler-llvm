; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @sexti8(i8 %rs1) nounwind {
; CHECK-LABEL: @sexti8
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SIGNEXTEND [[TMP:\$[0-9]+]], 0, [[IN1]]

  %res = sext i8 %rs1 to i256
  ret i256 %res
}

define i256 @sexti16(i16 %rs1) nounwind {
; CHECK-LABEL: @sexti16
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SIGNEXTEND [[TMP:\$[0-9]+]], 1, [[IN1]]

  %res = sext i16 %rs1 to i256
  ret i256 %res
}

define i256 @sexti32(i32 %rs1) nounwind {
; CHECK-LABEL: @sexti32
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SIGNEXTEND [[TMP:\$[0-9]+]], 3, [[IN1]]

  %res = sext i32 %rs1 to i256
  ret i256 %res
}

define i256 @sexti64(i64 %rs1) nounwind {
; CHECK-LABEL: @sexti64
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SIGNEXTEND [[TMP:\$[0-9]+]], 7, [[IN1]]

  %res = sext i64 %rs1 to i256
  ret i256 %res
}

define i256 @sexti128(i128 %rs1) nounwind {
; CHECK-LABEL: @sexti128
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SIGNEXTEND [[TMP:\$[0-9]+]], 15, [[IN1]]

  %res = sext i128 %rs1 to i256
  ret i256 %res
}
