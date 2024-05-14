; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

declare void @"$_"()
declare void @"_$_"()
declare void @"_$"()
declare void @"._"()
declare void @"_._"()
declare void @"_."()

; CHECK-LABEL: test
define void @test() {
; CHECK: call r0, @$_, @DEFAULT_UNWIND
  call void @"$_"()
; CHECK: call r0, @_$_, @DEFAULT_UNWIND
  call void @"_$_"()
; CHECK: call r0, @_$, @DEFAULT_UNWIND
  call void @"_$"()
; CHECK: call r0, @._, @DEFAULT_UNWIND
  call void @"._"()
; CHECK: call r0, @_._, @DEFAULT_UNWIND
  call void @"_._"()
; CHECK: call r0, @_., @DEFAULT_UNWIND
  call void @"_."()
  ret void
}
