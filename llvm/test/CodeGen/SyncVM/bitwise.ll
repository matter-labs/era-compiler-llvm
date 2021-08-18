; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: xorc
define i256 @xorc(i256 %par) nounwind {
; CHECK: sfll #340282366920938463463374607431768211455, r2, r2
; CHECK: sflh #340282366920938463463374607431768211455, r2, r2
; CHECK: xor r1, r2, r1
  %1 = xor i256 %par, -1
  ret i256 %1
}

; CHECK-LABEL: xor2
define i256 @xor2(i256 %arg1, i256 %arg2) nounwind {
; CHECK xor r1, r2, r1
  %1 = xor i256 %arg1, %arg2
  ret i256 %1
}

; CHECK-LABEL: andc
define i256 @andc(i256 %par) nounwind {
; CHECK: and #340282366920938463463374607431768211452, r1, r1
  %1 = and i256 %par, 340282366920938463463374607431768211452
  ret i256 %1
}

; CHECK-LABEL: and2
define i256 @and2(i256 %a, i256 %b) {
; CHECK: and r1, r2, r1
  %1 = and i256 %a, %b
  ret i256 %1
}

; CHECK-LABEL: orc
define i256 @orc(i256 %a) {
; CHECK: or #42, r1, r1
  %1 = or i256 %a, 42
  ret i256 %1
}

; CHECK-LABEL: or2
define i256 @or2(i256 %a, i256 %b) {
; CHECK: or r1, r2, r1
  %1 = or i256 %a, %b
  ret i256 %1
}

; CHECK-LABEL: select_and
define i256 @select_and(i1 %x, i1 %y, i256 %v) nounwind {
; TODO: For some reason `and` unrolls in additional control flow,
; which probably needs to be fixed
; CHECK: sub r2, r0, r0
  %1 = and i1 %x, %y
  %2 = select i1 %1, i256 %v, i256 42
  ret i256 %2
}

; CHECK-LABEL: select_or
define i256 @select_or(i1 %x, i1 %y, i256 %v) nounwind {
; TODO: For some reason `or` unrolls in additional control flow,
; which probably needs to be fixed
; CHECK: sub r2, r0, r0
  %1 = or i1 %x, %y
  %2 = select i1 %1, i256 %v, i256 42
  ret i256 %2
}

; CHECK-LABEL: select_xor
define i256 @select_xor(i1 %x, i1 %y, i256 %v) nounwind {
; CHECK: xor r1, r2, r2
; CHECK: sub r2, r0, r0
  %1 = xor i1 %x, %y
  %2 = select i1 %1, i256 %v, i256 42
  ret i256 %2
}
