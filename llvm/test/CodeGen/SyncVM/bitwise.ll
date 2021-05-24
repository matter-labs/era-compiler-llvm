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
; CHECK: sfll #8, r2, r2
; CHECK: sflh #0, r2, r2
; CHECK: div r1, r2, r0, r2
; CHECK: sfll #0, r3, r3
; CHECK: sflh #1, r3, r3
; CHECK: div r1, r3, r0, r3
; CHECK: sub r3, r2, r2
; CHECK: sfll #4, r3, r3
; CHECK: sflh #0, r3, r3
; CHECK: div r1, r3, r0, r1
; CHECK: add r2, r1, r1
  %1 = and i256 %par, 340282366920938463463374607431768211452
  ret i256 %1
}
