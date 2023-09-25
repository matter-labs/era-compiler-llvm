; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32"
target triple = "eravm"

%str.ty = type {i256, i8, i256}

; CHECK-LABEL: array_ldst_to_parameter
define void @array_ldst_to_parameter([10 x i256]* %array, i256 %val) {
  %idx = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 6
  %1 = load i256, i256* %idx
  %2 = add i256 %1, %val
  %idx2 = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 2
  store i256 %2, i256* %idx2
; CHECK: shr.s 5, r1, r1
; CHECK: add stack[6 + r1], r2, stack[2 + r1]
  ret void
}

; CHECK-LABEL: frame_compound_idx
define i256 @frame_compound_idx(i256 %val) {
  %1 = alloca [5 x i256]
  %2 = getelementptr inbounds [5 x i256], [5 x i256]* %1, i256 0, i256 2
; CHECK: add r1, r0, stack-[1]
; CHECK: add r1, r0, stack-[4]
  store i256 %val, i256* %2
  call void @foo()
; CHECK: add stack-[1], r0, r1
  %3 = load i256, i256* %2
  ret i256 %3
}

; CHECK: frame_compound
define i256 @frame_compound(i256 %index, i256 %val) {
  %1 = alloca [5 x i256]
  %2 = getelementptr inbounds [5 x i256], [5 x i256]* %1, i256 0, i256 %index
; CHECK: add r2, r0, stack-[1]
; CHECK: context.sp r3
; CHECK: add r3, r1, r1
; CHECK: sub.s 6, r1, r1
; CHECK: add r2, r0, stack[r1]
  store i256 %val, i256* %2
  call void @foo()
; CHECK: add stack-[1], r0, r1
  %3 = load i256, i256* %2
  ret i256 %3
}

declare void @foo()
