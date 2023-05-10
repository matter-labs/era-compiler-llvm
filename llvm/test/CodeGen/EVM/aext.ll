; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i8 @aexti8(i8 %rs1) nounwind {
; CHECK-LABEL: @aexti8
; CHECK: ARGUMENT [[IN:\$[0-9]+]], 0
; CHECK: CONST_I256 [[C:\$[0-9]+]], 1
; CHECK: ADD {{.*}}, [[IN]], [[C]]

  %res = add i8 %rs1, 1
  ret i8 %res
}
