; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm-unknown-unknown"

; CHECK-LABEL: zeroinit_fatptr
define void @zeroinit_fatptr() {
entry:
  ; CHECK: nop stack+=[2]
  %ptr = alloca { i8 addrspace(3)*, i1 }
  ; TODO: zero-initializing i1 is not optimal
  ; CHECK: ptr.add r0, r0, stack-[2]
  store { i8 addrspace(3)*, i1 } zeroinitializer, { i8 addrspace(3)*, i1 }* %ptr
  ret void
  ; CHECK: nop stack-=[2]
}
