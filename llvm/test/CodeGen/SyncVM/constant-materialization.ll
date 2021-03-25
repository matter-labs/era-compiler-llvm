; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:16:8-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: small_constant
define i256 @small_constant(i256 %a) nounwind {
; CHECK: movl	42, r2
; CHECK: movh	0, r2
  %1 = add i256 %a, 42
  ret i256 %1
}

; CHECK-LABEL: big_constant
define i256 @big_constant(i256 %a) nounwind {
; CHECK: movl	340282366920938463463374607431768211455, r2
; CHECK: movh	340282366920938463463374607431768211455, r2
  %1 = add i256 -1, %a
  ret i256 %1
}
