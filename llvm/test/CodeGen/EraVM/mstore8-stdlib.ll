; RUN: opt -O2 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @__mstore8(i256 addrspace(1)* nocapture nofree noundef dereferenceable(32) %addr, i256 %val) #0

define void @test(i256 addrspace(1)* %addr) nounwind {
; CHECK-LABEL: @test
; CHECK: load i256
; CHECK-NEXT: or i256
; CHECK-NEXT: store i256
  call void @__mstore8(i256 addrspace(1)* %addr, i256 255)
  ret void
}

attributes #0 = { argmemonly mustprogress nofree norecurse nosync nounwind null_pointer_is_valid willreturn }
