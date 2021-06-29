; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: test
define private i64 @test() {
entry:
  %"5" = call i8 @main(i16 1, i8 1, i16 2, i8 1, i16 3, i8 1)
  store i64 0, i64 addrspace(1)* null, align 32
  unreachable
}

declare dso_local i8 @main(i16, i8, i16, i8, i16, i8)
