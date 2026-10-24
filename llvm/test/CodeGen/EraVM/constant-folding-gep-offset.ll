; RUN: opt -passes=instsimplify -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@g = addrspace(1) global [4 x i256] zeroinitializer

; Check that GEP offsets that don't fit 64 bits are folded in 256 bits.
define ptr addrspace(1) @test_index() {
; CHECK-LABEL: @test_index(
; CHECK-NEXT: ret ptr addrspace(1) inttoptr (i256 18446744073709551621 to ptr addrspace(1))
  %r = getelementptr i8, ptr addrspace(1) null, i256 18446744073709551621
  ret ptr addrspace(1) %r
}

define ptr addrspace(1) @test_stride() {
; CHECK-LABEL: @test_stride(
; CHECK-NEXT: ret ptr addrspace(1) inttoptr (i256 36893488147419103232 to ptr addrspace(1))
  %r = getelementptr [4 x i256], ptr addrspace(1) null, i256 0, i256 1152921504606846976
  ret ptr addrspace(1) %r
}

define ptr addrspace(1) @test_nested() {
; CHECK-LABEL: @test_nested(
; CHECK-NEXT: ret ptr addrspace(1) inttoptr (i256 18446744073709551622 to ptr addrspace(1))
  %r = getelementptr i8, ptr addrspace(1) getelementptr (i8, ptr addrspace(1) null, i256 18446744073709551621), i256 1
  ret ptr addrspace(1) %r
}

define ptr addrspace(1) @test_nested_global() {
; CHECK-LABEL: @test_nested_global(
; CHECK-NEXT: ret ptr addrspace(1) getelementptr (i8, ptr addrspace(1) @g, i256 18446744073709551680)
  %r = getelementptr i256, ptr addrspace(1) getelementptr (i256, ptr addrspace(1) @g, i256 576460752303423489), i256 1
  ret ptr addrspace(1) %r
}

; Check that negative indices are still folded (this one was correct before).
define ptr addrspace(1) @test_negative() {
; CHECK-LABEL: @test_negative(
; CHECK-NEXT: ret ptr addrspace(1) inttoptr (i256 -64 to ptr addrspace(1))
  %r = getelementptr i256, ptr addrspace(1) getelementptr (i256, ptr addrspace(1) null, i256 -1), i256 -1
  ret ptr addrspace(1) %r
}
