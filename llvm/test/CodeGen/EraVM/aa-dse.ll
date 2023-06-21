; UNSUPPORTED: evm

; RUN: opt -passes=dse -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define void @test() {
; CHECK-LABEL: @test(
; CHECK-NEXT:    [[PTR2:%.*]] = inttoptr i256 32 to ptr addrspace(1)
; CHECK-NEXT:    store i256 1, ptr addrspace(1) [[PTR2]], align 64
; CHECK-NEXT:    ret void
;
  %ptr1 = inttoptr i256 32 to ptr addrspace(1)
  %ptr2 = inttoptr i256 32 to ptr addrspace(1)
  store i256 2, ptr addrspace(1) %ptr1, align 64
  store i256 1, ptr addrspace(1) %ptr2, align 64
  ret void
}
