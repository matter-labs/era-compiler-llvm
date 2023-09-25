; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK-LABEL: brcond:
define i256 @brcond(i256 %p1) nounwind {
; CHECK: and! 1, r1, r1
; CHECK-NEXT: add.eq  43, r0, r1
; CHECK-NEXT: add.ne  42, r0, r1
; CHECK-NEXT: ret
  %1 = trunc i256 %p1 to i1
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 43
}
