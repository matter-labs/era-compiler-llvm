; RUN: llc -O3 -eravm-jump-table-density-threshold=30 < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test(i256 %a, i256 %b, i256 %cond) {
; CHECK-LABEL: test:
; CHECK:         jump.le @JTI0_0[r3]
; CHECK-NEXT:    jump @.BB0_6
entry:
  switch i256 %cond, label %return [
    i256 0, label %add
    i256 1, label %sub
    i256 2, label %mul
    i256 10, label %divcheck
  ]

add:
  %addres = add nsw i256 %a, %b
  br label %return

sub:
  %subres = sub nsw i256 %a, %b
  br label %return

mul:
  %mulres = mul nsw i256 %a, %b
  br label %return

divcheck:
  %cmp = icmp eq i256 %b, 0
  br i1 %cmp, label %return, label %div

div:
  %divres = udiv i256 %a, %b
  br label %return

return:
  %res = phi i256 [ %mulres, %mul ], [ %subres, %sub ], [ %addres, %add ], [ %divres, %div ], [ 0, %divcheck ], [ 0, %entry ]
  ret i256 %res
}

; CHECK:       JTI0_0:
; CHECK-NEXT:  .cell @.BB0_1
; CHECK-NEXT:  .cell @.BB0_2
; CHECK-NEXT:  .cell @.BB0_3
; CHECK-NEXT:  .cell @.BB0_6
; CHECK-NEXT:  .cell @.BB0_6
; CHECK-NEXT:  .cell @.BB0_6
; CHECK-NEXT:  .cell @.BB0_6
; CHECK-NEXT:  .cell @.BB0_6
; CHECK-NEXT:  .cell @.BB0_6
; CHECK-NEXT:  .cell @.BB0_6
; CHECK-NEXT:  .cell @.BB0_4
