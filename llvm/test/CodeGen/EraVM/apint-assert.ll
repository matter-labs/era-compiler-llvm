; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK-LABEL: test
define i256 @test(i256 %0) {
entry:
; CHECK: stm.h	r1, r2
  %1 = add i256 %0, 18446744073709551615
  %2 = inttoptr i256 %1 to ptr addrspace(1)
  store i8 0, ptr addrspace(1) %2, align 1
  ret i256 0
}
