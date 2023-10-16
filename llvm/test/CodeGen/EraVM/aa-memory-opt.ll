; RUN: opt -O3 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @llvm.memcpy.p1.p1.i256(ptr addrspace(1), ptr addrspace(1), i256, i1 immarg)

define i256 @test_memcpy() {
; CHECK-LABEL: @test_memcpy(
; CHECK-NEXT:    store i256 2, ptr addrspace(1) inttoptr (i256 2 to ptr addrspace(1)), align 64
; CHECK-NEXT:    tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) noundef nonnull align 32 dereferenceable(53) inttoptr (i256 96 to ptr addrspace(1)), ptr addrspace(1) noundef nonnull align 256 dereferenceable(53) inttoptr (i256 256 to ptr addrspace(1)), i256 53, i1 false)
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(1) inttoptr (i256 2 to ptr addrspace(1)), align 64
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 96 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 256 to ptr addrspace(1)), i256 53, i1 false)
  %ret = load i256, ptr addrspace(1) inttoptr (i256 2 to ptr addrspace(1)), align 64
  ret i256 %ret
}

define i256 @test_different_locsizes() {
; CHECK-LABEL: @test_different_locsizes(
; CHECK-NEXT:    store i256 2, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 64
; CHECK-NEXT:    store i8 1, ptr addrspace(2) inttoptr (i8 1 to ptr addrspace(2)), align 64
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 64
  store i8 1, ptr addrspace(2) inttoptr (i8 1 to ptr addrspace(2)), align 64
  %ret = load i256, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 64
  ret i256 %ret
}

define i256 @test_as() {
; CHECK-LABEL: @test_as(
; CHECK-NEXT:    store i256 2, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 64
; CHECK-NEXT:    store i256 1, ptr addrspace(1) inttoptr (i256 1 to ptr addrspace(1)), align 64
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 64
  store i256 1, ptr addrspace(1) inttoptr (i256 1 to ptr addrspace(1)), align 64
  %ret = load i256, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 64
  ret i256 %ret
}

define i256 @test_as1_overlap() {
; CHECK-LABEL: @test_as1_overlap(
; CHECK-NEXT:    store i256 2, ptr addrspace(1) null, align 4294967296
; CHECK-NEXT:    store i256 1, ptr addrspace(1) inttoptr (i256 31 to ptr addrspace(1)), align 64
; CHECK-NEXT:    [[RET:%.*]] = load i256, ptr addrspace(1) null, align 4294967296
; CHECK-NEXT:    ret i256 [[RET]]
;
  store i256 2, ptr addrspace(1) null, align 64
  store i256 1, ptr addrspace(1) inttoptr (i256 31 to ptr addrspace(1)), align 64
  %ret = load i256, ptr addrspace(1) null, align 64
  ret i256 %ret
}

define i256 @test_as1_null() {
; CHECK-LABEL: @test_as1_null(
; CHECK-NEXT:    store i256 2, ptr addrspace(1) null, align 4294967296
; CHECK-NEXT:    store i256 1, ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1)), align 64
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(1) null, align 64
  store i256 1, ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1)), align 64
  %ret = load i256, ptr addrspace(1) null, align 64
  ret i256 %ret
}

define i256 @test_as1_small() {
; CHECK-LABEL: @test_as1_small(
; CHECK-NEXT:    store i256 2, ptr addrspace(1) inttoptr (i256 33 to ptr addrspace(1)), align 64
; CHECK-NEXT:    store i256 1, ptr addrspace(1) inttoptr (i256 1 to ptr addrspace(1)), align 64
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(1) inttoptr (i256 33 to ptr addrspace(1)), align 64
  store i256 1, ptr addrspace(1) inttoptr (i256 1 to ptr addrspace(1)), align 64
  %ret = load i256, ptr addrspace(1) inttoptr (i256 33 to ptr addrspace(1)), align 64
  ret i256 %ret
}

define i256 @test_as1_large() {
; CHECK-LABEL: @test_as1_large(
; CHECK-NEXT:    store i256 2, ptr addrspace(1) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533664 to ptr addrspace(1)), align 64
; CHECK-NEXT:    store i256 1, ptr addrspace(1) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533632 to ptr addrspace(1)), align 4294967296
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(1) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533664 to ptr addrspace(1)), align 64
  store i256 1, ptr addrspace(1) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533632 to ptr addrspace(1)), align 64
  %ret = load i256, ptr addrspace(1) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533664 to ptr addrspace(1)), align 64
  ret i256 %ret
}

define i256 @test_as2_overlap() {
; CHECK-LABEL: @test_as2_overlap(
; CHECK-NEXT:    store i256 2, ptr addrspace(2) null, align 4294967296
; CHECK-NEXT:    store i256 1, ptr addrspace(2) inttoptr (i256 31 to ptr addrspace(2)), align 64
; CHECK-NEXT:    [[RET:%.*]] = load i256, ptr addrspace(2) null, align 4294967296
; CHECK-NEXT:    ret i256 [[RET]]
;
  store i256 2, ptr addrspace(2) null, align 64
  store i256 1, ptr addrspace(2) inttoptr (i256 31 to ptr addrspace(2)), align 64
  %ret = load i256, ptr addrspace(2) null, align 64
  ret i256 %ret
}

define i256 @test_as2_null() {
; CHECK-LABEL: @test_as2_null(
; CHECK-NEXT:    store i256 2, ptr addrspace(2) null, align 4294967296
; CHECK-NEXT:    store i256 1, ptr addrspace(2) inttoptr (i256 32 to ptr addrspace(2)), align 64
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(2) null, align 64
  store i256 1, ptr addrspace(2) inttoptr (i256 32 to ptr addrspace(2)), align 64
  %ret = load i256, ptr addrspace(2) null, align 64
  ret i256 %ret
}

define i256 @test_as2_small() {
; CHECK-LABEL: @test_as2_small(
; CHECK-NEXT:    store i256 2, ptr addrspace(2) inttoptr (i256 33 to ptr addrspace(2)), align 64
; CHECK-NEXT:    store i256 1, ptr addrspace(2) inttoptr (i256 1 to ptr addrspace(2)), align 64
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(2) inttoptr (i256 33 to ptr addrspace(2)), align 64
  store i256 1, ptr addrspace(2) inttoptr (i256 1 to ptr addrspace(2)), align 64
  %ret = load i256, ptr addrspace(2) inttoptr (i256 33 to ptr addrspace(2)), align 64
  ret i256 %ret
}

define i256 @test_as2_large() {
; CHECK-LABEL: @test_as2_large(
; CHECK-NEXT:    store i256 2, ptr addrspace(2) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533664 to ptr addrspace(2)), align 64
; CHECK-NEXT:    store i256 1, ptr addrspace(2) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533632 to ptr addrspace(2)), align 4294967296
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(2) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533664 to ptr addrspace(2)), align 64
  store i256 1, ptr addrspace(2) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533632 to ptr addrspace(2)), align 64
  %ret = load i256, ptr addrspace(2) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533664 to ptr addrspace(2)), align 64
  ret i256 %ret
}

define i256 @test_as5_null() {
; CHECK-LABEL: @test_as5_null(
; CHECK-NEXT:    store i256 2, ptr addrspace(5) null, align 4294967296
; CHECK-NEXT:    store i256 1, ptr addrspace(5) inttoptr (i256 1 to ptr addrspace(5)), align 64
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(5) null, align 64
  store i256 1, ptr addrspace(5) inttoptr (i256 1 to ptr addrspace(5)), align 64
  %ret = load i256, ptr addrspace(5) null, align 64
  ret i256 %ret
}

define i256 @test_as5_small() {
; CHECK-LABEL: @test_as5_small(
; CHECK-NEXT:    store i256 2, ptr addrspace(5) inttoptr (i256 2 to ptr addrspace(5)), align 64
; CHECK-NEXT:    store i256 1, ptr addrspace(5) inttoptr (i256 1 to ptr addrspace(5)), align 64
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(5) inttoptr (i256 2 to ptr addrspace(5)), align 64
  store i256 1, ptr addrspace(5) inttoptr (i256 1 to ptr addrspace(5)), align 64
  %ret = load i256, ptr addrspace(5) inttoptr (i256 2 to ptr addrspace(5)), align 64
  ret i256 %ret
}

define i256 @test_as5_large() {
; CHECK-LABEL: @test_as5_large(
; CHECK-NEXT:    store i256 2, ptr addrspace(5) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533632 to ptr addrspace(5)), align 4294967296
; CHECK-NEXT:    store i256 1, ptr addrspace(5) inttoptr (i256 1 to ptr addrspace(5)), align 64
; CHECK-NEXT:    ret i256 2
;
  store i256 2, ptr addrspace(5) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533632 to ptr addrspace(5)), align 64
  store i256 1, ptr addrspace(5) inttoptr (i256 1 to ptr addrspace(5)), align 64
  %ret = load i256, ptr addrspace(5) inttoptr (i256 53919893334301279589334030174039261352344891250716429051063678533632 to ptr addrspace(5)), align 64
  ret i256 %ret
}
