; RUN: llc -opaque-pointers < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK-LABEL: foo
define void @foo() {
entry:
  %input = alloca [10 x i256], align 32
  br label %body4.i

body4.i:                                          ; preds = %br_true_59_15.i, %entry
  %0 = add nuw nsw i8 0, 1
  %1 = zext i8 %0 to i64
  %2 = getelementptr [10 x i256], [10 x i256]* %input, i64 0, i64 %1
  br label %br_true_59_15.i

br_true_59_15.i:                                  ; preds = %body4.i
  store i256 undef, i256* %2, align 32
  br label %body4.i
}
