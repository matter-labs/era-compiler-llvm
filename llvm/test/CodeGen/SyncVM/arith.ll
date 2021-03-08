; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:16:8-i256:256:256"
target triple = "tvm"

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
