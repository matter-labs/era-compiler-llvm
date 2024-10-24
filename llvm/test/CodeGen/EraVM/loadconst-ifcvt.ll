; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -O3 < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test(i1 %cond) {
; CHECK-LABEL: test:
; CHECK:       ; %bb.0: ; %entry
; CHECK-NEXT:    add r1, r0, r2
; CHECK-NEXT:    add code[@CPI0_0], r0, r1
; CHECK-NEXT:    sub! r2, r0, r0
; CHECK-NEXT:    add.ne code[@CPI0_1], r0, r1
; CHECK-NEXT:    ret
entry:
  br i1 %cond, label %bb1, label %exit

bb1:
  br label %exit

exit:
  %phi = phi i256 [ 123456789, %bb1 ], [ 987654321, %entry ]
  ret i256 %phi
}
