; RUN: llc -O0 < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: foo
define dso_local void @foo() {
entry:
  %0 = alloca i8, align 32
  %1 = bitcast i8* %0 to i256*
  br label %condition

condition:                                        ; preds = %condition, %entry
  %2 = getelementptr i256, i256* %1, i256 undef
  br label %condition
}
