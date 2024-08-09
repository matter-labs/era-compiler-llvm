; RUN: llc --disable-eravm-scalar-opt-passes < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK-LABEL: array_ldst_to_parameter
define void @array_ldst_to_parameter([10 x i256]* %array, i256 %val) {
  ; CHECK: incsp 10
  %starr = alloca [10 x i256]
  %idx = getelementptr inbounds [10 x i256], [10 x i256]* %starr, i256 0, i256 6
  %1 = load i256, i256* %idx
  %2 = add i256 %1, %val
  %idx2 = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 2
  ; CHECK: add stack-[4], r2, stack[2 + r1]
  store i256 %2, i256* %idx2
  %idx3 = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 1
  %3 = load i256, i256* %idx3
  %idx4 = getelementptr inbounds [10 x i256], [10 x i256]* %starr, i256 0, i256 1
  ; CHECK: add stack[1 + r1], r0, stack-[9]
  store i256 %3, i256* %idx4
  ret void
}

; CHECK-LABEL: retptr
define { [10 x i256], i256 }* @retptr({ [10 x i256], i256 }* returned align 32 %0) {
entry:
  ; CHECK-NOT: r1
  ; CHECK: ret
  ret { [10 x i256], i256 }* %0
}

; CHECK-LABEL: retptr2
define { [10 x i256], i256 }* @retptr2({ [10 x i256], i256 }* returned align 32 %0) {
entry:
  br i1 undef, label %join, label %body
body:                                             ; preds = %entry
  unreachable
join:                                             ; preds = %entry
  ret { [10 x i256], i256 }* %0
}

; CHECK-LABEL: caller
define i256* @caller() {
  %alloc = alloca i256
  %alloc2 = alloca i256
; CHECK: sp r1
; CHECK: sp r2
; CHECK: sub.s 3, r2, r2
; CHECK: shl.s 5, r2, r2
; CHECK: add r2, r0, stack[1 + r1]
  %v = call i256* @callee(i256* %alloc, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256* %alloc2)
  ret i256* %v
}

; CHECK-LABEL: callee
define i256* @callee(i256* %ptr, i256 %a1, i256 %a2, i256 %a3, i256 %a4, i256 %a5, i256 %a6, i256 %a7, i256 %a8, i256 %a9, i256 %a10, i256 %a11, i256 %a12, i256 %a13, i256 %a14, i256* %ptr2) {
; CHECK: shr.s 5, r1, r3
; CHECK: add r2, r0, stack[r3]
  store i256 %a1, i256* %ptr
; CHECK: add stack-[1], r0, r3
; CHECK: shr.s 5, r3, r3
; CHECK: add r2, r0, stack[r3]
  store i256 %a1, i256* %ptr2
  ret i256* %ptr
}
