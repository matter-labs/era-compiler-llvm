; RUN: opt -S < %s -passes=instcombine | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256-ni:1"

define ptr @test_neg() {
; CHECK-LABEL: @test_neg(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    ret ptr inttoptr (i256 192 to ptr)
;
entry:
  %gep = getelementptr inbounds i8, ptr inttoptr (i256 224 to ptr), i256 -32
  ret ptr %gep
}

define ptr addrspace(1) @test_inner_neg() {
; CHECK-LABEL: @test_inner_neg(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    ret ptr addrspace(1) getelementptr (i8, ptr addrspace(1) null, i256 -22)
;
entry:
  %gep = getelementptr i8, ptr addrspace(1) getelementptr inbounds (i8, ptr addrspace(1) null, i256 -32), i256 10
  ret ptr addrspace(1) %gep
}
