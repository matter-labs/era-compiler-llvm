; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:16:8-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: addi256
define i256 @addi256(i256 %par, i256 %p2) nounwind {
; CHECK: add r1, r1, r2
  %1 = add i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: subi256
define i256 @subi256(i256 %par, i256 %p2) nounwind {
; CHECK: sub r1, r1, r2
  %1 = sub i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: muli256
define i256 @muli256(i256 %par, i256 %p2) nounwind {
; CHECK: mul r1, r0, r1, r2
  %1 = mul i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: udivi256
define i256 @udivi256(i256 %par, i256 %p2) nounwind {
; CHECK: div r1, r0, r1, r2
  %1 = udiv i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: uremi256
define i256 @uremi256(i256 %par, i256 %p2) nounwind {
; CHECK: div r0, r1, r1, r2
  %1 = urem i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: udivremi256
; TODO: optimize to use a single div instruction
define i256 @udivremi256(i256 %par, i256 %p2) nounwind {
; CHECK: div r0, r3, r1, r2
; CHECK: div r1, r0, r1, r2
; CHECK: add r1, r1, r3
  %1 = udiv i256 %par, %p2
  %2 = urem i256 %par, %p2
  %3 = add i256 %1, %2
  ret i256 %3
}
