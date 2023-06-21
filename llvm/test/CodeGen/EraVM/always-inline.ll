; RUN: opt -passes=eravm-always-inline -S < %s | FileCheck %s

UNSUPPORTED: evm

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK: Function Attrs: alwaysinline
; CHECK-LABEL: @inline
define void @inline() {
  ret void
}

; CHECK-NOT: Function Attrs: alwaysinline
; CHECK-LABEL: @noinline
define void @noinline() {
  ret void
}

; CHECK-NOT: Function Attrs: alwaysinline
; CHECK-LABEL: @test
define void @test() {
  call void @inline()
  call void @noinline()
  call void @noinline()
  ret void
}

; CHECK-NOT: Function Attrs: alwaysinline
; CHECK-LABEL: @callattr
define void @callattr() {
  ret void
}

; CHECK-NOT: Function Attrs: alwaysinline
; CHECK-LABEL: @test_noinline_callattr
define void @test_noinline_callattr() {
  call void @callattr() noinline
  ret void
}

; CHECK-NOT: Function Attrs: alwaysinline
; CHECK-LABEL: @test_noinline_recursion
define void @test_noinline_recursion() {
  call void @test_noinline_recursion()
  ret void
}
