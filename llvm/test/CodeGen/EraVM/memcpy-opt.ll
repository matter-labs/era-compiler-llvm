; RUN: opt -passes=memcpyopt -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @llvm.memcpy.p1.p1.i256(ptr addrspace(1), ptr addrspace(1), i256, i1 immarg)

define i256 @test() {
; CHECK-LABEL: @test(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 96 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 256 to ptr addrspace(1)), i256 53, i1 false)
; CHECK-NEXT:    tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 288 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 96 to ptr addrspace(1)), i256 53, i1 false)
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 96 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 256 to ptr addrspace(1)), i256 53, i1 false)
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 288 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 96 to ptr addrspace(1)), i256 53, i1 false)
  ret i256 0
}
