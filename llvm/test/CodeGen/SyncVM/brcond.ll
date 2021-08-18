; RUN: llc -O0 < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: brcond:
define i256 @brcond(i256 %p1) nounwind {
; CHECK: add r1, r0, r2
; CHECK: and #1, r2, r2
; CHECK: sfll #1, r3, r3
; CHECK: sflh #0, r3, r3
; CHECK: sub	r2, r3, r0
; CHECK: je	.LBB0_1, .LBB0_2
  %1 = trunc i256 %p1 to i1
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 43
}

; CHECK-LABEL: bug.21.05.04
define void @bug.21.05.04() {
entry:
  br label %condition

condition:                                        ; preds = %entry
; CHECK: sub r{{[0-9]}}, r0, r0
; CHECK: jne .LBB1_3, .LBB1_2
  br i1 undef, label %body, label %join

body:                                             ; preds = %condition
  unreachable

join:                                             ; preds = %condition
  unreachable
}

; CHECK-LABEL: negflag
define i256 @negflag() nounwind {
; CHECK: call foo
; CHECK-NEXT: jlt .LBB2_1, .LBB2_2
  call void @foo()
  %1 = tail call i256 @llvm.syncvm.ltflag()
  %2 = and i256 %1, 1
  %3 = icmp ne i256 %2, 0
  br i1 %3, label %l1, label %l2
l1:
  ret i256 1
l2:
  ret i256 42
}

declare void @foo()
declare i256 @llvm.syncvm.ltflag()
