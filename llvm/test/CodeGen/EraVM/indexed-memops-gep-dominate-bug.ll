; RUN: llc -O3 -stop-after=verify -compile-twice=false < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test(i256 %arg, ptr addrspace(1) %arg1, ptr addrspace(1) %arg2) {
; CHECK-LABEL: @test(
entry:
  %icmp1 = icmp eq i256 %arg, 0
  br i1 %icmp1, label %bb7, label %bb1

bb1:
  br label %bb2

bb2:
  %phi1 = phi i256 [ %add, %bb5 ], [ 0, %bb1 ]
  %icmp2 = icmp ugt i256 %phi1, 100
  br i1 %icmp2, label %bb3, label %bb4

bb3:
  ; CHECK: getelementptr inbounds i256, ptr addrspace(1) %arg1, i256 %phi1
  %gep1 = getelementptr inbounds i256, ptr addrspace(1) %arg1, i256 %phi1
  store i256 5, ptr addrspace(1) %gep1, align 32
  br label %bb5

bb4:
  ; CHECK: getelementptr inbounds i256, ptr addrspace(1) %arg2, i256 %phi1
  %gep2 = getelementptr inbounds i256, ptr addrspace(1) %arg2, i256 %phi1
  store i256 10, ptr addrspace(1) %gep2, align 32
  br label %bb5

bb5:
  %add = add i256 %phi1, 1
  %cmp3 = icmp eq i256 %add, 0
  br i1 %cmp3, label %bb6, label %bb2

bb6:
  br label %bb7

bb7:
  %phi2 = phi i256 [ 0, %entry ], [ %add, %bb6 ]
  ret i256 %phi2
}

; Check that the GEP isn't rewritten if it dominates the latch, but the store
; doesn't, as the increment is inserted before the store.
define void @test_memop_not_dominating_latch(ptr addrspace(1) %arg, i256 %n, i256 %v) {
; CHECK-LABEL: @test_memop_not_dominating_latch(
entry:
  br label %header

header:
  %phi = phi i256 [ 0, %entry ], [ %add, %latch ]
  ; CHECK: getelementptr inbounds i256, ptr addrspace(1) %arg, i256 %phi
  %gep = getelementptr inbounds i256, ptr addrspace(1) %arg, i256 %phi
  %and = and i256 %phi, 1
  %icmp1 = icmp eq i256 %and, 0
  br i1 %icmp1, label %then, label %latch

then:
  store i256 %v, ptr addrspace(1) %gep, align 32
  br label %latch

latch:
  %add = add nuw i256 %phi, 1
  %icmp2 = icmp eq i256 %add, %n
  br i1 %icmp2, label %exit, label %header

exit:
  ret void
}
