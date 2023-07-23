; RUN: llc < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm-unknown-unknown"

; check that when base pointer is a stack address (passed down as argument in cells),
; it is scaled for pointer arithmetics.
; CHECK-LABEL: return_elem_var
define ptr @return_elem_var(ptr %array, i256 %i) {
; CHECK:      shl.s   5, r2, r2
;	CHECK-NEXT: mul	    32, r1, r[[REG:[0-9]+]], r0
; CHECK-NEXT: add     r[[REG]], r2
  %addri = getelementptr inbounds [10 x i256], ptr %array, i256 0, i256 %i
  store i256 0, ptr %addri
  ret ptr %array
}

; pointer arithmetic result as return value
; CHECK-LABEL: return_elem_var_2
define ptr @return_elem_var_2(i256* %array) nounwind {
  %addri = getelementptr inbounds [10 x i256], ptr %array, i256 0, i256 7
  store i256 0, ptr %addri
; CHECK:      mul	32, r1, r1, r0
; CHECK-NEXT: add	224, r1, r1
; CHECK-NOT: div.s 32, r1, r1, r0
; CHECK-NEXT: ret
  ret ptr %addri
}

; return stack pointer. return cell pointer as is
; CHECK-LABEL: return_ptr
define ptr @return_ptr(i256* %array) nounwind {
  ; CHECK-NOT: mul
  ret ptr %array
}

declare void @foo(ptr %val);

; passing stack pointer in cells as argument
; CHECK-LABEL: call_foo
define void @call_foo() nounwind {
; CHECK: context.sp      [[REG:r[0-9]+]]
; CHECK: sub.s   1, [[REG]], [[REG2:r[0-9]+]]
; CHECK-NOT: mul
; CHECK-NOT: div.s
; CHECK: near_call
  %p = alloca i256
  call void @foo(ptr %p)
  ret void
}

declare ptr @bar();

; passing stack pointer in cells as argument
; CHECK-LABEL: call_bar
define ptr @call_bar() nounwind {
; CHECK-NOT: mul
; CHECK-NOT: div
  %p = call ptr @bar();
  ret ptr %p
}

; passing stack pointer in cells as argument
; CHECK-LABEL: call_bar2
define ptr @call_bar2() nounwind {
  %p = call ptr @bar();
  %p2 = getelementptr inbounds [10 x i256], ptr %p, i256 0, i256 7
; CHECK-NOT: div.s   32, [[REG_BAR:r[0-9]+]], r1, r0
  store i256 0, ptr %p2
  ret ptr %p2
}

; passing stack pointer
; CHECK-LABEL: call_bar3
define ptr @call_bar3() nounwind {
; CHECK-NOT:      mul 32, r1, r1, r0
; CHECK: add 224, r1, r1
; CHECK-NOT: div.s 32, r1, r1, r0
; CHECK: ret
  %p = call ptr @bar();
  %p2 = getelementptr inbounds [10 x i256], ptr %p, i256 0, i256 7
  ret ptr %p2
}


; complicated case of pointer arithmetics, and return values
; CHECK-LABEL: ZKSYNC_NEAR_CALL_16_args_tuple20
define ptr @ZKSYNC_NEAR_CALL_16_args_tuple20(ptr %0, i256 %1, i256 %2, i256 %3, i256 %4, i256 %5, i256 %6, i256 %7, i256 %8, i256 %9, i256 %10, i256 %11, i256 %12, i256 %13, i256 %14, i256 %15, i256 %16) {
entry:
  %return_0_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 0
  store i256 0, ptr %return_0_gep_pointer, align 32
  %return_1_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 1
  store i256 0, ptr %return_1_gep_pointer, align 32
  %return_2_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 2
  store i256 0, ptr %return_2_gep_pointer, align 32
  %return_3_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 3
  store i256 0, ptr %return_3_gep_pointer, align 32
  %return_4_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 4
  store i256 0, ptr %return_4_gep_pointer, align 32
  %return_5_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 5
  store i256 0, ptr %return_5_gep_pointer, align 32
  %return_6_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 6
  store i256 0, ptr %return_6_gep_pointer, align 32
  %return_7_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 7
  store i256 0, ptr %return_7_gep_pointer, align 32
  %return_8_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 8
  store i256 0, ptr %return_8_gep_pointer, align 32
  %return_9_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 9
  store i256 0, ptr %return_9_gep_pointer, align 32
  %return_10_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 10
  store i256 0, ptr %return_10_gep_pointer, align 32
  %return_11_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 11
  store i256 0, ptr %return_11_gep_pointer, align 32
  %return_12_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 12
  store i256 0, ptr %return_12_gep_pointer, align 32
  %return_13_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 13
  store i256 0, ptr %return_13_gep_pointer, align 32
  %return_14_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 14
  store i256 0, ptr %return_14_gep_pointer, align 32
  %return_15_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 15
  store i256 0, ptr %return_15_gep_pointer, align 32
  %return_16_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 16
  store i256 0, ptr %return_16_gep_pointer, align 32
  %return_17_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 17
  store i256 0, ptr %return_17_gep_pointer, align 32
  %return_18_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 18
  store i256 0, ptr %return_18_gep_pointer, align 32
  %return_19_gep_pointer = getelementptr { i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256 }, ptr %0, i256 0, i32 19
  store i256 0, ptr %return_19_gep_pointer, align 32
  %arg1 = alloca i256, align 32
  store i256 %1, ptr %arg1, align 32
  %arg2 = alloca i256, align 32
  store i256 %2, ptr %arg2, align 32
  %arg3 = alloca i256, align 32
  store i256 %3, ptr %arg3, align 32
  %arg4 = alloca i256, align 32
  store i256 %4, ptr %arg4, align 32
  %arg5 = alloca i256, align 32
  store i256 %5, ptr %arg5, align 32
  %arg6 = alloca i256, align 32
  store i256 %6, ptr %arg6, align 32
  %arg7 = alloca i256, align 32
  store i256 %7, ptr %arg7, align 32
  %arg8 = alloca i256, align 32
  store i256 %8, ptr %arg8, align 32
  %arg9 = alloca i256, align 32
  store i256 %9, ptr %arg9, align 32
  %arg10 = alloca i256, align 32
  store i256 %10, ptr %arg10, align 32
  %arg11 = alloca i256, align 32
  store i256 %11, ptr %arg11, align 32
  %arg12 = alloca i256, align 32
  store i256 %12, ptr %arg12, align 32
  %arg13 = alloca i256, align 32
  store i256 %13, ptr %arg13, align 32
  %arg14 = alloca i256, align 32
  store i256 %14, ptr %arg14, align 32
  %arg15 = alloca i256, align 32
  store i256 %15, ptr %arg15, align 32
  %arg16 = alloca i256, align 32
  store i256 %16, ptr %arg16, align 32
  br label %invoke_success_block

return:                                           ; preds = %invoke_success_block
  ret ptr %0

invoke_success_block:                             ; preds = %entry
  %arg17 = load i256, ptr %arg1, align 32
  store i256 %arg17, ptr %return_0_gep_pointer, align 32
  %arg28 = load i256, ptr %arg2, align 32
  store i256 %arg28, ptr %return_1_gep_pointer, align 32
  %arg39 = load i256, ptr %arg3, align 32
  store i256 %arg39, ptr %return_2_gep_pointer, align 32
  %arg410 = load i256, ptr %arg4, align 32
  store i256 %arg410, ptr %return_3_gep_pointer, align 32
  %arg511 = load i256, ptr %arg5, align 32
  store i256 %arg511, ptr %return_4_gep_pointer, align 32
  %arg612 = load i256, ptr %arg6, align 32
  store i256 %arg612, ptr %return_5_gep_pointer, align 32
  %arg713 = load i256, ptr %arg7, align 32
  store i256 %arg713, ptr %return_6_gep_pointer, align 32
  %arg814 = load i256, ptr %arg8, align 32
  store i256 %arg814, ptr %return_7_gep_pointer, align 32
  %arg915 = load i256, ptr %arg9, align 32
  store i256 %arg915, ptr %return_8_gep_pointer, align 32
  %arg1016 = load i256, ptr %arg10, align 32
  store i256 %arg1016, ptr %return_9_gep_pointer, align 32
  %arg1117 = load i256, ptr %arg11, align 32
  store i256 %arg1117, ptr %return_10_gep_pointer, align 32
  %arg1218 = load i256, ptr %arg12, align 32
  store i256 %arg1218, ptr %return_11_gep_pointer, align 32
  %arg1319 = load i256, ptr %arg13, align 32
  store i256 %arg1319, ptr %return_12_gep_pointer, align 32
  %arg1420 = load i256, ptr %arg14, align 32
  store i256 %arg1420, ptr %return_13_gep_pointer, align 32
  %arg1521 = load i256, ptr %arg15, align 32
  store i256 %arg1521, ptr %return_14_gep_pointer, align 32
  %arg1622 = load i256, ptr %arg16, align 32
  store i256 %arg1622, ptr %return_15_gep_pointer, align 32
  store i256 17, ptr %return_16_gep_pointer, align 32
  store i256 18, ptr %return_17_gep_pointer, align 32
  store i256 19, ptr %return_18_gep_pointer, align 32
  store i256 20, ptr %return_19_gep_pointer, align 32
  br label %return
}

