; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: diamond
define i256 @diamond(i256 %p1, i256 %p2, i256 %x, i256 %y) nounwind {
  %1 = icmp ugt i256 %p1, %p2
  ; CHECK: sub! r1, r2, r{{[0-9]+}}
  ; CHECK: jump.le @.BB0_2
  ; CHECK: jump @.BB0_3
  br i1 %1, label %l1, label %l2
l1:
  %2 = add i256 %p1, %x
  br label %exit
l2:
  %3 = add i256 %p2, %y
  br label %exit
exit:
  %4 = phi i256 [ %2, %l1  ], [ %3, %l2 ]
  ret i256 %4
}
