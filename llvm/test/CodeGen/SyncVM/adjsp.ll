target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: array_ldst_to_parameter
define void @array_ldst_to_parameter([10 x i256]* %array, i256 %val) {
  %starr = alloca [10 x i256]
  %idx = getelementptr inbounds [10 x i256], [10 x i256]* %starr, i256 0, i256 6
; CHECK: mov 6(r1), r3
  %1 = load i256, i256* %idx
; CHECK: add r3, r2, r2
  %2 = add i256 %1, %val
  %idx2 = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 2
; CHECK: mov r2, 2(r1)
  store i256 %2, i256* %idx2
  %idx3 = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 1
  %3 = load i256, i256* %idx3
  %idx4 = getelementptr inbounds [10 x i256], [10 x i256]* %starr, i256 0, i256 1
  store i256 %3, i256* %idx4
  ret void
}
