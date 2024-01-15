; RUN: opt -O3 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test_this() {
; CHECK-LABEL: @test_this(
; CHECK-NEXT:    [[VAL1:%.*]] = tail call i256 @llvm.eravm.this()
; CHECK-NEXT:    [[RES:%.*]] = shl i256 [[VAL1]], 1
; CHECK-NEXT:    ret i256 [[RES]]
;
  %val1 = call i256 @llvm.eravm.this()
  %val2 = call i256 @llvm.eravm.this()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_caller() {
; CHECK-LABEL: @test_caller(
; CHECK-NEXT:    [[VAL1:%.*]] = tail call i256 @llvm.eravm.caller()
; CHECK-NEXT:    [[RES:%.*]] = shl i256 [[VAL1]], 1
; CHECK-NEXT:    ret i256 [[RES]]
;
  %val1 = call i256 @llvm.eravm.caller()
  %val2 = call i256 @llvm.eravm.caller()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_codesource() {
; CHECK-LABEL: @test_codesource(
; CHECK-NEXT:    [[VAL1:%.*]] = tail call i256 @llvm.eravm.codesource()
; CHECK-NEXT:    [[RES:%.*]] = shl i256 [[VAL1]], 1
; CHECK-NEXT:    ret i256 [[RES]]
;
  %val1 = call i256 @llvm.eravm.codesource()
  %val2 = call i256 @llvm.eravm.codesource()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_meta() {
; CHECK-LABEL: @test_meta(
; CHECK-NEXT:    [[VAL1:%.*]] = tail call i256 @llvm.eravm.meta()
; CHECK-NEXT:    [[VAL2:%.*]] = tail call i256 @llvm.eravm.meta()
; CHECK-NEXT:    [[RES:%.*]] = add i256 [[VAL2]], [[VAL1]]
; CHECK-NEXT:    ret i256 [[RES]]
;
  %val1 = call i256 @llvm.eravm.meta()
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_meta_dce() {
; CHECK-LABEL: @test_meta_dce(
; CHECK-NEXT:    ret i256 0
;
  %val1 = call i256 @llvm.eravm.meta()
  ret i256 0
}

define i256 @test_txorigin() {
; CHECK-LABEL: @test_txorigin(
; CHECK-NEXT:    [[VAL1:%.*]] = tail call i256 @llvm.eravm.txorigin()
; CHECK-NEXT:    [[RES:%.*]] = shl i256 [[VAL1]], 1
; CHECK-NEXT:    ret i256 [[RES]]
;
  %val1 = call i256 @llvm.eravm.txorigin()
  %val2 = call i256 @llvm.eravm.txorigin()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_gasleft() {
; CHECK-LABEL: @test_gasleft(
; CHECK-NEXT:    [[VAL1:%.*]] = tail call i256 @llvm.eravm.gasleft()
; CHECK-NEXT:    [[VAL2:%.*]] = tail call i256 @llvm.eravm.gasleft()
; CHECK-NEXT:    [[RES:%.*]] = add i256 [[VAL2]], [[VAL1]]
; CHECK-NEXT:    ret i256 [[RES]]
;
  %val1 = call i256 @llvm.eravm.gasleft()
  %val2 = call i256 @llvm.eravm.gasleft()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_gasleft_dce() {
; CHECK-LABEL: @test_gasleft_dce(
; CHECK-NEXT:    ret i256 0
;
  %val1 = call i256 @llvm.eravm.gasleft()
  ret i256 0
}

define i256 @test_getu128() {
; CHECK-LABEL: @test_getu128(
; CHECK-NEXT:    [[VAL1:%.*]] = tail call i256 @llvm.eravm.getu128()
; CHECK-NEXT:    [[RES:%.*]] = shl i256 [[VAL1]], 1
; CHECK-NEXT:    ret i256 [[RES]]
;
  %val1 = call i256 @llvm.eravm.getu128()
  %val2 = call i256 @llvm.eravm.getu128()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_getu128_dce() {
; CHECK-LABEL: @test_getu128_dce(
; CHECK-NEXT:    ret i256 0
;
  %val1 = call i256 @llvm.eravm.getu128()
  ret i256 0
}

define void @test_setu128(i256 %val) {
; CHECK-LABEL: @test_setu128(
; CHECK-NEXT:    tail call void @llvm.eravm.setu128(i256 [[VAL:%.*]])
; CHECK-NEXT:    tail call void @llvm.eravm.setu128(i256 [[VAL]])
; CHECK-NEXT:    ret void
;
  call void @llvm.eravm.setu128(i256 %val)
  call void @llvm.eravm.setu128(i256 %val)
  ret void
}

define i256 @test_getu128_setu128(i256 %val) {
; CHECK-LABEL: @test_getu128_setu128(
; CHECK-NEXT:    [[VAL1:%.*]] = tail call i256 @llvm.eravm.getu128()
; CHECK-NEXT:    tail call void @llvm.eravm.setu128(i256 [[VAL:%.*]])
; CHECK-NEXT:    [[RES:%.*]] = shl i256 [[VAL1]], 1
; CHECK-NEXT:    ret i256 [[RES]]
;
  %val1 = call i256 @llvm.eravm.getu128()
  call void @llvm.eravm.setu128(i256 %val)
  %val2 = call i256 @llvm.eravm.getu128()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define void @test_inctx() {
; CHECK-LABEL: @test_inctx(
; CHECK-NEXT:    tail call void @llvm.eravm.inctx()
; CHECK-NEXT:    tail call void @llvm.eravm.inctx()
; CHECK-NEXT:    ret void
;
  call void @llvm.eravm.inctx()
  call void @llvm.eravm.inctx()
  ret void
}

define void @test_setprice(i256 %val) {
; CHECK-LABEL: @test_setprice(
; CHECK-NEXT:    tail call void @llvm.eravm.setpubdataprice(i256 [[VAL:%.*]])
; CHECK-NEXT:    tail call void @llvm.eravm.setpubdataprice(i256 [[VAL]])
; CHECK-NEXT:    ret void
;
  call void @llvm.eravm.setpubdataprice(i256 %val)
  call void @llvm.eravm.setpubdataprice(i256 %val)
  ret void
}

declare i256 @llvm.eravm.this()
declare i256 @llvm.eravm.caller()
declare i256 @llvm.eravm.codesource()
declare i256 @llvm.eravm.meta()
declare i256 @llvm.eravm.txorigin()
declare i256 @llvm.eravm.gasleft()
declare i256 @llvm.eravm.getu128()
declare void @llvm.eravm.setu128(i256)
declare void @llvm.eravm.inctx()
declare void @llvm.eravm.setpubdataprice(i256)
