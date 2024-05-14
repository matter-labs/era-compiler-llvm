; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

; CHECK-LABEL: zeroinit_fatptr
define void @zeroinit_fatptr() {
entry:
  ; CHECK: incsp 2
  %ptr = alloca { i8 addrspace(3)*, i1 }
  ; TODO: CPR-888 Codegen for store to stack-[1] is incorrect
  ; CHECK: add 0, r0, stack-[2]
  store { i8 addrspace(3)*, i1 } zeroinitializer, { i8 addrspace(3)*, i1 }* %ptr, align 32
  ret void
}
