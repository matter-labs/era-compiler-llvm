; RUN: llc --mtriple=evm < %s | FileCheck %s

define i8 @aexti8(i8 %rs1) nounwind {
; CHECK-LABEL: @aexti8
; CHECK: ARGUMENT [[IN:\$[0-9]+]], 0
; CHECK: CONST_I256 [[C:\$[0-9]+]], 1
; CHECK: ADD {{.*}}, [[IN]], [[C]]

  %res = add i8 %rs1, 1
  ret i8 %res
}
