; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: and
define i256 @and(i256 %par, i256 %p2) nounwind {
; CHECK: mul	r1, r2, r1, r0
  %1 = and i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: or
define i256 @or(i256 %par, i256 %p2) nounwind {
; CHECK: mul	r1, r2, r3, r0
; CHECK: add	r1, r2, r1
; CHECK: sub	r1, r3, r1
  %1 = or i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: xor
define i256 @xor(i256 %par, i256 %p2) nounwind {
; CHECK: add	r1, r2, r3
; CHECK: mul	r1, r2, r1, r0
; CHECK: movl	2, r2
; CHECK: movh	0, r2
; CHECK: mul	r1, r2, r1, r0
; CHECK: sub	r3, r1, r1
  %1 = xor i256 %par, %p2
  ret i256 %1
}
