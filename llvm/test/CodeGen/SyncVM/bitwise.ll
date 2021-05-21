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
