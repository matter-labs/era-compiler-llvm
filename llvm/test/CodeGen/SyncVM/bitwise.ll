; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: xorneg1
define i256 @xorneg1(i256 %par) nounwind {
; CHECK: sub r0, r1, r1
; CHECK: sfll #340282366920938463463374607431768211455, r2, r2
; CHECK: sflh #340282366920938463463374607431768211455, r2, r2
; CHECK: add r1, r2, r1
  %1 = xor i256 %par, -1
  ret i256 %1
}

; CHECK-LABEL: and
define i256 @and(i256 %par) nounwind {
; CHECK: sfll #0, r2, r2
; CHECK: sflh #1, r2, r2
; CHECK: div r1, r2, r0, r2
; CHECK: div r1, #4, r0, r1
; CHECK: sub r2, r1, r1
  %1 = and i256 %par, 340282366920938463463374607431768211452
  ret i256 %1
}

; CHECK-LABEL: and2
define i256 @and2(i256 %par) nounwind {
; CHECK: div	r1, #32, r0, r2
; CHECK: div	r1, #65536, r0, r1
; CHECK: sub	r1, r2, r1
  %1 = and i256 %par, 65504
  ret i256 %1
}

; CHECK-LABEL: and3
define i8 @and3(i8 %in) {
; CHECK: div r1, #2, r0, r2
; CHECK: div r1, #256, r0, r1
; CHECK: sub r1, r2, r1
  %1 = and i8 %in, -2
  ret i8 %1
}

; CHECK-LABEL: select_and
define i256 @select_and(i1 %x, i1 %y, i256 %v) nounwind {
; CHECK: mul r1, r2, r1, r0
; CHECK: sub r1, r0, r0
  %1 = and i1 %x, %y
  %2 = select i1 %1, i256 %v, i256 42
  ret i256 %2
}

; CHECK-LABEL: select_or
define i256 @select_or(i1 %x, i1 %y, i256 %v) nounwind {
; CHECK: mul r1, r2, r4, r0
; CHECK: add r1, r2, r1
; CHECK: sub r1, r4, r1
; CHECK: sub r1, r0, r0
  %1 = or i1 %x, %y
  %2 = select i1 %1, i256 %v, i256 42
  ret i256 %2
}

; CHECK-LABEL: select_xor
define i256 @select_xor(i1 %x, i1 %y, i256 %v) nounwind {
; CHECK: div r1, #2, r0, r1
; CHECK: sub r1, r0, r0
  %1 = xor i1 %x, %y
  %2 = select i1 %1, i256 %v, i256 42
  ret i256 %2
}
