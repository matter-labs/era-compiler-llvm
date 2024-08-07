; XFAIL: *
; RUN: llc --stop-after=eravm-indexed-memops-prepare --compile-twice=false < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

define i256 @test(ptr addrspace(2) %addr, i1 %cond) {
; CHECK-LABEL: @test(
bb:
  br label %bb2

bb2:                                              ; preds = %bb7, %bb
  br label %bb3

bb3:                                              ; preds = %bb2
  br i1 false, label %bb7, label %bb4

bb4:                                              ; preds = %bb6, %bb3
  %phi = phi i256 [ %add, %bb6 ], [ 0, %bb3 ]
  ; CHECK: getelementptr i8, ptr addrspace(2) %addr, i256 %phi
  %gep = getelementptr i8, ptr addrspace(2) %addr, i256 %phi
  %load = load i256, ptr addrspace(2) %gep, align 32
  br i1 %cond, label %bb6, label %bb5

bb5:                                              ; preds = %bb4
  unreachable

bb6:                                              ; preds = %bb4
  %add = add i256 %phi, 32
  br i1 false, label %bb4, label %bb7

bb7:                                              ; preds = %bb6, %bb3
  br label %bb2
}
