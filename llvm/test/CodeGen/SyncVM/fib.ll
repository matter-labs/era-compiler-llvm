; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: main
define i64 @main(i8 %0) nounwind {
entry:
  %1 = add i8 %0, -2
  %.not15 = icmp eq i8 %1, 0
; CHECK: sub.s!
; CHECK: jump.eq
  br i1 %.not15, label %join, label %body

body:                                             ; preds = %entry, %body
  %i.018 = phi i8 [ %3, %body ], [ 1, %entry ]
  %value_2.017 = phi i64 [ %2, %body ], [ 1, %entry ]
  %value_1.016 = phi i64 [ %value_2.017, %body ], [ 0, %entry ]
  %2 = add i64 %value_2.017, %value_1.016
  %3 = add i8 %i.018, 1
  %.not = icmp ugt i8 %3, %1
; CHECK: sub
; CHECK: jump.le
  br i1 %.not, label %join, label %body

join:                                             ; preds = %body, %entry
  %fibo.0.lcssa = phi i64 [ 0, %entry ], [ %2, %body ]
  ret i64 %fibo.0.lcssa
}
