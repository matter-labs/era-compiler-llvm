; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: brcond
define i256 @brcond(i256 %p1) nounwind {
; CHECK: sub	r1, r0, r0
; CHECK: je	.LBB0_2, .LBB0_1
  %1 = trunc i256 %p1 to i1
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 43
}
