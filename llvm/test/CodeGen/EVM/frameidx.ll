; RUN: llc --evm-keep-registers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i256 @alloca() nounwind {
; CHECK-LABEL: alloca:
; CHECK: STACK_LOAD [[RES:\$[0-9]+]], %SP

  %var = alloca i256, align 1
  %rv = load i256, ptr %var
  ret i256 %rv
}

define i256 @alloca2() nounwind {
; CHECK-LABEL: alloca2:
; CHECK: STACK_LOAD [[RES:\$[0-9]+]], %SP, 4096

  %fill = alloca i256, i32 128
  %var = alloca i256, align 1
  %rv = load i256, ptr %var
  ret i256 %rv
}

define void @alloca3(i256 %val) nounwind {
; CHECK-LABEL: alloca3:
; CHECK: ARGUMENT [[VAL:\$[0-9]+]], 0
; CHECK: STACK_STORE %SP, 0, [[VAL]]

  %fill = alloca i256, align 1
  store i256 %val, ptr %fill
  ret void
}

define i256 @alloca4() nounwind {
; CHECK-LABEL: alloca4:
; CHECK: STACK_LOAD [[RES:\$[0-9]+]], %SP, 64

  %alloca_ptr = alloca i256, i32 128
  %elm = getelementptr i256, ptr %alloca_ptr, i256 2
  %rv = load i256, ptr %elm
  ret i256 %rv
}

define void @alloca5(i256 %val) nounwind {
; CHECK-LABEL: alloca5:
; CHECK: STACK_STORE %SP, 64, [[RES:\$[0-9]+]]

  %alloca_ptr = alloca i256, i32 128
  %elm = getelementptr i256, ptr %alloca_ptr, i256 2
  store i256 %val, ptr %elm
  ret void
}
