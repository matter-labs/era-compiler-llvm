; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

%str.ty = type {i256, i8, i256}
@arr = internal global [5 x i256] [i256 1, i256 2, i256 3, i256 4, i256 5], align 256
; TODO: arrays with unaligned elements are not properly supported yet.
@arr2 = internal global [5 x i64] [i64 1, i64 2, i64 3, i64 4, i64 5], align 256
@struct = internal global %str.ty {i256 1, i8 2, i256 3}, align 256

; CHECK-LABEL: array_ldst_to_parameter
define void @array_ldst_to_parameter([10 x i256]* %array, i256 %val) {
  %idx = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 6
; CHECK: mov 6(r1), r3
  %1 = load i256, i256* %idx
; CHECK: add r3, r2, r2
  %2 = add i256 %1, %val
  %idx2 = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 2
; CHECK: mov r2, 2(r1)
  store i256 %2, i256* %idx2
  ret void
}

; CHECK-LABEL: write_to_global
define void @write_to_global(i256 %index, i256 %val) {
; CHECK: sfll #32, r3, r3
; CHECK: sflh #0, r3, r3
; CHECK: mul r1, r3, r1, r0
  %idx = getelementptr inbounds [5 x i256], [5 x i256]* @arr, i256 0, i256 %index
; CHECK: mov r2, arr(r1)
  store i256 %val, i256* %idx
  ret void
}

; CHECK-LABEL: read_from_global
define i256 @read_from_global(i256 %index, i256 %val) {
; CHECK: sfll #32, r2, r2
; CHECK: sflh #0, r2, r2
; CHECK: mul r1, r2, r1, r0
  %idx = getelementptr inbounds [5 x i256], [5 x i256]* @arr, i256 0, i256 %index
; CHECK: mov arr(r1), r1
  %1 = load i256, i256* %idx
  ret i256 %1
}

; CHECK-LABEL: frame_compound_idx
define i256 @frame_compound_idx(i256 %val) {
  %1 = alloca [5 x i256]
  %2 = getelementptr inbounds [5 x i256], [5 x i256]* %1, i256 0, i256 2
; CHECK: mov r1, 3(sp)
  store i256 %val, i256* %2
  call void @foo()
; CHECK: mov 3(sp), r1
  %3 = load i256, i256* %2
  ret i256 %3
}

; CHECK-LABEL: struct_sum
define i256 @struct_sum() {
; CHECK-DAG: mov struct, r2
  %1 = load i256, i256* getelementptr inbounds (%str.ty, %str.ty* @struct, i256 0, i32 0), align 256
; CHECK-DAG: mov struct+32, r1
  %2 = load i8, i8* getelementptr inbounds (%str.ty, %str.ty* @struct, i256 0, i32 1), align 256
  %3 = zext i8 %2 to i256
  %4 = add i256 %1, %3
; CHECK: mov struct+64, r2
  %5 = load i256, i256* getelementptr inbounds (%str.ty, %str.ty* @struct, i256 0, i32 2), align 256
  %6 = add i256 %4, %5
  ret i256 %6
}

; CHECK: frame_compound
define i256 @frame_compound(i256 %index, i256 %val) {
  %1 = alloca [5 x i256]
  %2 = getelementptr inbounds [5 x i256], [5 x i256]* %1, i256 0, i256 %index
  ; CHECK: mov r2, 2(sp-r1)
  store i256 %val, i256* %2
  call void @foo()
	; CHECK: mov 2(sp-r1), r1
  %3 = load i256, i256* %2
  ret i256 %3
}

declare void @foo()

; CHECK: arr
; CHECK: struct
