; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: and
define i1 @and(i1 %a1, i1 %a2) nounwind {
; CHECK: mul r1, r2, r1, r0
  %1 = and i1 %a1, %a2
  ret i1 %1
}

; CHECK-LABEL: or
define i1 @or(i1 %a1, i1 %a2) nounwind {
; CHECK: mul r1, r2, r3, r0
; CHECK: add r1, r2, r1
; CHECK: sub r1, r3, r1
  %1 = or i1 %a1, %a2
  ret i1 %1
}

; CHECK-LABEL: neg
define i1 @neg(i1 %a1) nounwind {
; CHECK: sfll #1, r2, r2
; CHECK: sflh #0, r2, r2
; CHECK: add r1, r2, r1
  %1 = xor i1 true, %a1
  ret i1 %1
}

; CHECK-LABEL: xor
define i1 @xor(i1 %a1, i1 %a2) nounwind {
; CHECK: add r1, r2, r1
  %1 = xor i1 %a1, %a2
  ret i1 %1
}
