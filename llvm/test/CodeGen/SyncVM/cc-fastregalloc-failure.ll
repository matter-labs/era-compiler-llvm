
; RUN: llc < %s | FileCheck %s
target datalayout = "e-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: test
define private i64 @test() {
entry:
  %"5" = call i8 @foo(i16 1, i8 1, i16 2, i8 1, i16 3, i8 1, i16 0, i8 1)
  ; CHECK: pop #1, r0
  unreachable
}

declare i8 @foo(i16, i8, i16, i8, i16, i8, i16, i8)
