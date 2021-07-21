; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: addi256
define i256 @addi256(i256 %par, i256 %p2) nounwind {
; CHECK: add r1, r2, r1
  %1 = add i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: subi256
define i256 @subi256(i256 %par, i256 %p2) nounwind {
; CHECK: sub r1, r2, r1
  %1 = sub i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: muli256
define i256 @muli256(i256 %par, i256 %p2) nounwind {
; CHECK: mul r1, r2, r1, r0
  %1 = mul i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: udivi256
define i256 @udivi256(i256 %par, i256 %p2) nounwind {
; CHECK: div r1, r2, r1, r0
  %1 = udiv i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: uremi256
define i256 @uremi256(i256 %par, i256 %p2) nounwind {
; CHECK: div r1, r2, r0, r1
  %1 = urem i256 %par, %p2
  ret i256 %1
}

; CHECK-LABEL: udivremi256
; TODO: optimize to use a single div instruction
define i256 @udivremi256(i256 %par, i256 %p2) nounwind {
; CHECK: div r1, r2, r0, r3
; CHECK: div r1, r2, r2, r0
; CHECK: add r2, r3, r1
  %1 = udiv i256 %par, %p2
  %2 = urem i256 %par, %p2
  %3 = add i256 %1, %2
  ret i256 %3
}

; non i256 types
; CHECK-LABEL: addi64
define i256 @addi64(i64 %p1, i64 %p2) nounwind {
; CHECK: add r1, r2, r2
  %1 = add i64 %p1, %p2
; CHECK: div r2, #18446744073709551616, r0, r1
  %2 = zext i64 %1 to i256
  ret i256 %2
}
