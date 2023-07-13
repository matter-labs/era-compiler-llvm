; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @alloca() nounwind {
; CHECK-LABEL: alloca:
; CHECK: STACK_LOAD [[RES:\$[0-9]+]], %SP

  %var = alloca i256, align 1
  %rv = load i256, ptr %var
  ret i256 %rv
}

define i256 @alloca2() nounwind {
; CHECK-LABEL: alloca2:
; CHECK: CONST_I256 [[FILL:\$[0-9]+]], 4096
; CHECK: ADD [[PTR:\$[0-9]+]], %SP, [[FILL]]
; CHECK: STACK_LOAD [[RES:\$[0-9]+]], [[PTR]]

  %fill = alloca i256, i32 128
  %var = alloca i256, align 1
  %rv = load i256, ptr %var
  ret i256 %rv
}

define void @alloca3(i256 %val) nounwind {
; CHECK-LABEL: alloca3:
; CHECK: ARGUMENT [[VAL:\$[0-9]+]], 0
; CHECK: STACK_STORE %SP, [[VAL]]

  %fill = alloca i256, align 1
  store i256 %val, ptr %fill
  ret void
}

define i256 @alloca4() nounwind {
; CHECK-LABEL: alloca4:
; CHECK: CONST_I256 [[OFF:\$[0-9]+]], 64
; CHECK: ADD [[PTR:\$[0-9]+]], %SP, [[OFF]]
; CHECK: STACK_LOAD [[RES:\$[0-9]+]], [[PTR]]

  %alloca_ptr = alloca i256, i32 128
  %elm = getelementptr i256, ptr %alloca_ptr, i256 2
  %rv = load i256, ptr %elm
  ret i256 %rv
}

define void @alloca5(i256 %val) nounwind {
; CHECK-LABEL: alloca5:
; CHECK: ARGUMENT [[ARG1:\$[0-9]]], 0
; CHECK: CONST_I256 [[OFF:\$[0-9]+]], 64
; CHECK: ADD [[PTR:\$[0-9]+]], %SP, [[OFF]]
; CHECK: STACK_STORE [[PTR]], [[RES:\$[0-9]+]]

  %alloca_ptr = alloca i256, i32 128
  %elm = getelementptr i256, ptr %alloca_ptr, i256 2
  store i256 %val, ptr %elm
  ret void
}
