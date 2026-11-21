; RUN: opt -passes=correlated-propagation -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; Check that CorrelatedValuePropagation does not mark the reachable default
; destination of an i256 switch as unreachable when ReachableCaseCount and CR
; are out of sync (pr142286).
define i256 @test_cvp_switch_reachable_case_count() {
; CHECK-LABEL: @test_cvp_switch_reachable_case_count(
; CHECK-NOT:   default.unreachable
; CHECK:       ret i256 42
;
start:
  br label %loop

loop:
  %phi = phi i256 [ -1, %start ], [ 0, %loop3 ]
  br label %loop2

loop2:
  br label %loop3

loop3:
  switch i256 %phi, label %exit [
    i256 0, label %loop3
    i256 1, label %loop2
    i256 2, label %loop
  ]

exit:
  ret i256 42
}
