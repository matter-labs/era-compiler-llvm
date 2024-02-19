; RUN: opt -O3 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @llvm.memmove.p1.p1.i256(ptr addrspace(1), ptr addrspace(1), i256, i1 immarg)
declare void @llvm.memcpy.p1.p1.i256(ptr addrspace(1), ptr addrspace(1), i256, i1 immarg)

define i256 @test_memmove_1() {
; CHECK-LABEL: @test_memmove_1(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i8, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i8 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 1, i1 false)
  ret i256 0
}

define i256 @test_memmove_2() {
; CHECK-LABEL: @test_memmove_2(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i16, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i16 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 2, i1 false)
  ret i256 0
}

define i256 @test_memmove_4() {
; CHECK-LABEL: @test_memmove_4(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i32, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i32 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 4, i1 false)
  ret i256 0
}

define i256 @test_memmove_8() {
; CHECK-LABEL: @test_memmove_8(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i64, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i64 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 8, i1 false)
  ret i256 0
}

define i256 @test_memmove_16() {
; CHECK-LABEL: @test_memmove_16(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i128, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i128 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 16, i1 false)
  ret i256 0
}

define i256 @test_memmove_32() {
; CHECK-LABEL: @test_memmove_32(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i256, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i256 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 32, i1 false)
  ret i256 0
}

define i256 @test_memmove_64_not() {
; CHECK-LABEL: @test_memmove_64_not(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) noundef nonnull align 2 dereferenceable(64) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) noundef nonnull align 4 dereferenceable(64) inttoptr (i256 100 to ptr addrspace(1)), i256 64, i1 false)
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 64, i1 false)
  ret i256 0
}

define i256 @test_memcpy_1() {
; CHECK-LABEL: @test_memcpy_1(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i8, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i8 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 1, i1 false)
  ret i256 0
}

define i256 @test_memcpy_2() {
; CHECK-LABEL: @test_memcpy_2(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i16, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i16 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 2, i1 false)
  ret i256 0
}

define i256 @test_memcpy_4() {
; CHECK-LABEL: @test_memcpy_4(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i32, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i32 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 4, i1 false)
  ret i256 0
}

define i256 @test_memcpy_8() {
; CHECK-LABEL: @test_memcpy_8(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i64, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i64 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 8, i1 false)
  ret i256 0
}

define i256 @test_memcpy_16() {
; CHECK-LABEL: @test_memcpy_16(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i128, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i128 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 16, i1 false)
  ret i256 0
}

define i256 @test_memcpy_32() {
; CHECK-LABEL: @test_memcpy_32(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i256, ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), align 4
; CHECK-NEXT:    store i256 [[TMP0]], ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), align 2
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 32, i1 false)
  ret i256 0
}

define i256 @test_memcpy_64_not() {
; CHECK-LABEL: @test_memcpy_64_not(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) noundef nonnull align 2 dereferenceable(64) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) noundef nonnull align 4 dereferenceable(64) inttoptr (i256 100 to ptr addrspace(1)), i256 64, i1 false)
; CHECK-NEXT:    ret i256 0
;
entry:
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 64, i1 false)
  ret i256 0
}
