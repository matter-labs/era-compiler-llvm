; RUN: llc -O3 < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test(i256 %cond) {
; CHECK-LABEL: test:
; CHECK:         sub.s! 3, r1, r0
; CHECK-NEXT:    jump.le @JTI0_0[r1]
; CHECK:       .BB0_1:
; CHECK-NEXT:    add r0, r0, r1
; CHECK-NEXT:    ret
; CHECK:       .BB0_2:
; CHECK-NEXT:    add 1, r0, r1
; CHECK-NEXT:    ret
; CHECK:       .BB0_3:
; CHECK-NEXT:    add 2, r0, r1
; CHECK-NEXT:    ret
; CHECK:       .BB0_4:
; CHECK-NEXT:    add 3, r0, r1
; CHECK-NEXT:    ret
; CHECK:       .BB0_5:
; CHECK-NEXT:    add 4, r0, r1
; CHECK-NEXT:    ret
entry:
  switch i256 %cond, label %exit [
    i256 0, label %bb1
    i256 1, label %bb2
    i256 2, label %bb3
    i256 3, label %bb4
  ]

exit:
  %ret = phi i256 [ 4, %bb4 ], [ 3, %bb3 ], [ 2, %bb2 ], [ 1, %bb1 ], [ 0, %entry ]
  ret i256 %ret

bb1:
  br label %exit

bb2:
  br label %exit

bb3:
  br label %exit

bb4:
  br label %exit
}
