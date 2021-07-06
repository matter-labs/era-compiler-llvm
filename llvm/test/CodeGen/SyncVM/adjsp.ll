; RUN: llc < %s | FileCheck %s
target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: array_ldst_to_parameter
define void @array_ldst_to_parameter([10 x i256]* %array, i256 %val) {
  ; CHECK: push #9, r0
  %starr = alloca [10 x i256]
  %idx = getelementptr inbounds [10 x i256], [10 x i256]* %starr, i256 0, i256 6
  %1 = load i256, i256* %idx
  %2 = add i256 %1, %val
  %idx2 = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 2
  ; CHECK: mov r{{[1-6]}}, 13(sp-r{{[1-6]}})
  store i256 %2, i256* %idx2
  %idx3 = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 1
  ; CHECK: mov 12(sp-r{{[1-6]}}), r{{[1-6]}}
  %3 = load i256, i256* %idx3
  %idx4 = getelementptr inbounds [10 x i256], [10 x i256]* %starr, i256 0, i256 1
  store i256 %3, i256* %idx4
  ; CHECK: pop #9, r0
  ret void
}

; CHECK-LABEL: retptr
define { [10 x i256], i256 }* @retptr({ [10 x i256], i256 }* returned align 32 %0) {
entry:
; CHECK-NOT: # AdjSP
  ret { [10 x i256], i256 }* %0
}

; CHECK-LABEL: retptr2
define { [10 x i256], i256 }* @retptr2({ [10 x i256], i256 }* returned align 32 %0) {
; CHECK-NOT: # AdjSP
entry:
  br i1 undef, label %join, label %body
body:                                             ; preds = %entry
  unreachable
join:                                             ; preds = %entry
  ret { [10 x i256], i256 }* %0
}
