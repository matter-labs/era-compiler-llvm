; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: diamond
define i256 @diamond(i256 %p1, i256 %p2, i256 %x, i256 %y) nounwind {
  %1 = icmp ugt i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  %2 = add i256 %p1, %x
  br label %exit
l2:
  %3 = add i256 %p2, %y
; CHECK: j	@.LBB0_3, @.LBB0_3
  br label %exit
exit:
  %4 = phi i256 [ %2, %l1  ], [ %3, %l2 ]
  ret i256 %4
}
