; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK-LABEL: array_2d
define dso_local void @array_2d() {
entry:
  %array = alloca [4 x [4 x i256]], align 32
  br label %condition

condition:                                        ; preds = %join10, %entry
  br label %condition7

condition7:                                       ; preds = %condition
  br i1 undef, label %body8, label %join10

body8:                                            ; preds = %condition7
  %i15 = load i8, i8 addrspace(1)* undef, align 32
  %0 = zext i8 %i15 to i256
  %1 = getelementptr [4 x [4 x i256]], [4 x [4 x i256]]* %array, i256 0, i256 %0
  %j16 = load i8, i8 addrspace(1)* undef, align 32
  %2 = zext i8 %j16 to i256
  %3 = getelementptr [4 x i256], [4 x i256]* %1, i256 0, i256 %2
; CHECK: add 0, r0, stack[r{{[0-9]+}}]
  store i256 0, i256* %3, align 32
  unreachable

join10:                                           ; preds = %condition7
  br label %condition
}
