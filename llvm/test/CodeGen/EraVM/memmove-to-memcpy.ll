; RUN: opt -O3 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @llvm.memmove.p0.p0.i256(ptr, ptr, i256, i1 immarg)
declare void @llvm.memmove.p1.p1.i256(ptr addrspace(1), ptr addrspace(1), i256, i1 immarg)
declare void @llvm.memmove.p1.p2.i256(ptr addrspace(1), ptr addrspace(2), i256, i1 immarg)

define i256 @test_known_not() {
; CHECK-LABEL: @test_known_not(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) noundef nonnull align 2 dereferenceable(100) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) noundef nonnull align 4 dereferenceable(100) inttoptr (i256 100 to ptr addrspace(1)), i256 100, i1 false)
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 100, i1 false)
  ret i256 0
}

define i256 @test_unknown_not(ptr addrspace(1) %dst, ptr addrspace(1) %src, i256 %size) {
; CHECK-LABEL: @test_unknown_not(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) align 1 [[DST:%.*]], ptr addrspace(1) align 1 [[SRC:%.*]], i256 [[SIZE:%.*]], i1 false)
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) %dst, ptr addrspace(1) %src, i256 %size, i1 false)
  ret i256 0
}

define i256 @test_known() {
; CHECK-LABEL: @test_known(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) noundef nonnull align 2 dereferenceable(85) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) noundef nonnull align 4 dereferenceable(85) inttoptr (i256 100 to ptr addrspace(1)), i256 85, i1 false)
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 85, i1 false)
  ret i256 0
}

define i256 @test_known_different_as() {
; CHECK-LABEL: @test_known_different_as(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    tail call void @llvm.memcpy.p1.p2.i256(ptr addrspace(1) noundef nonnull align 2 dereferenceable(85) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(2) noundef nonnull align 2 dereferenceable(85) inttoptr (i256 10 to ptr addrspace(2)), i256 85, i1 false)
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p2.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(2) inttoptr (i256 10 to ptr addrspace(2)), i256 85, i1 false)
  ret i256 0
}

define i256 @test_unknown_different_as(ptr addrspace(1) %dst, ptr addrspace(2) %src, i256 %size) {
; CHECK-LABEL: @test_unknown_different_as(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    tail call void @llvm.memcpy.p1.p2.i256(ptr addrspace(1) align 1 [[DST:%.*]], ptr addrspace(2) align 1 [[SRC:%.*]], i256 [[SIZE:%.*]], i1 false)
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p2.i256(ptr addrspace(1) %dst, ptr addrspace(2) %src, i256 %size, i1 false)
  ret i256 0
}

define i256 @test_stack(ptr %dst) {
; CHECK-LABEL: @test_stack(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[SRC_SROA_0:%.*]] = alloca i256, align 32
; CHECK-NEXT:    store volatile i256 5, ptr [[SRC_SROA_0]], align 32
; CHECK-NEXT:    call void @llvm.memcpy.p0.p0.i256(ptr noundef nonnull align 1 dereferenceable(10) [[DST:%.*]], ptr noundef nonnull align 32 dereferenceable(10) [[SRC_SROA_0]], i256 10, i1 false)
; CHECK-NEXT:    ret i256 0
;
entry:
  %src = alloca [10 x i256], align 32
  store volatile i256 5, ptr %src, align 4
  tail call void @llvm.memmove.p0.p0.i256(ptr %dst, ptr %src, i256 10, i1 false)
  ret i256 0
}
