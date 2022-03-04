; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42
@val2 = addrspace(4) global i256 43

; CHECK-LABEL: contextr
define i256 @contextr() {
; CHECK-DAG: context.caller r{{[0-9]+}}
; CHECK-DAG: context.self_address r{{[0-9]+}}
; CHECK-DAG: context.code_address r{{[0-9]+}}
; CHECK-DAG: context.meta r{{[0-9]+}}
; CHECK-DAG: context.tx_origin r{{[0-9]+}}
; CHECK-DAG: context.coinbase r{{[0-9]+}}
; CHECK-DAG: context.ergs_left r{{[0-9]+}}
  %1 = call i256 @llvm.syncvm.context(i256 0)
  %2 = call i256 @llvm.syncvm.context(i256 1)
  %3 = call i256 @llvm.syncvm.context(i256 2)
  %4 = call i256 @llvm.syncvm.context(i256 3)
  %5 = call i256 @llvm.syncvm.context(i256 4)
  %6 = call i256 @llvm.syncvm.context(i256 5)
  %7 = call i256 @llvm.syncvm.ergsleft()
  %8 = add i256 %1, %2
  %9 = add i256 %8, %3
  %10 = add i256 %9, %4
  %11 = add i256 %10, %5
  %12 = add i256 %11, %6
  %13 = add i256 %12, %7
  ret i256 %13
}

; CHECK-LABEL: contexts
define void @contexts() {
; CHECK-DAG: context.caller stack-[6]
; CHECK-DAG: context.self_address stack-[7]
; CHECK-DAG: context.code_address stack-[5]
; CHECK-DAG: context.meta stack-[4]
; CHECK-DAG: context.tx_origin stack-[3]
; CHECK-DAG: context.coinbase stack-[2]
; CHECK-DAG: context.ergs_left stack-[1]
  %ptr1 = alloca i256
  %ptr2 = alloca i256
  %ptr3 = alloca i256
  %ptr4 = alloca i256
  %ptr5 = alloca i256
  %ptr6 = alloca i256
  %ptr7 = alloca i256
  %1 = call i256 @llvm.syncvm.context(i256 0)
  %2 = call i256 @llvm.syncvm.context(i256 1)
  %3 = call i256 @llvm.syncvm.context(i256 2)
  %4 = call i256 @llvm.syncvm.context(i256 3)
  %5 = call i256 @llvm.syncvm.context(i256 4)
  %6 = call i256 @llvm.syncvm.context(i256 5)
  %7 = call i256 @llvm.syncvm.ergsleft()
  store i256 %1, i256* %ptr1, align 32
  store i256 %2, i256* %ptr2, align 32
  store i256 %3, i256* %ptr3, align 32
  store i256 %4, i256* %ptr4, align 32
  store i256 %5, i256* %ptr5, align 32
  store i256 %6, i256* %ptr6, align 32
  store i256 %7, i256* %ptr7, align 32
  ret void
}

; CHECK-LABEL: sload_rr
define i256 @sload_rr(i256 %val) {
; CHECK-DAG: sload r1, r{{[0-9]+}}
; CHECK-DAG: sload.first r1, r{{[0-9]+}}
  %1 = call i256 @llvm.syncvm.sload(i256 %val, i256 0)
  %2 = call i256 @llvm.syncvm.sload(i256 %val, i256 1)
  %3 = add i256 %2, %1
  ret i256 %3
}

; CHECK-LABEL: sstore_r
define void @sstore_r(i256 %key, i256 %val) {
; CHECK: sstore r1, r2
; CHECK: sstore.first r1, r2
  call void @llvm.syncvm.sstore(i256 %key, i256 %val, i256 0)
  call void @llvm.syncvm.sstore(i256 %key, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: tol1_r
define void @tol1_r(i256 %key, i256 %val) {
; CHECK: to_l1 r1, r2
; CHECK: to_l1.first r1, r2
  call void @llvm.syncvm.tol1(i256 %key, i256 %val, i256 0)
  call void @llvm.syncvm.tol1(i256 %key, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: event_r
define void @event_r(i256 %key, i256 %val) {
; CHECK: event r1, r2
; CHECK: event.first r1, r2
  call void @llvm.syncvm.event(i256 %key, i256 %val, i256 0)
  call void @llvm.syncvm.event(i256 %key, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: precompile_r
define void @precompile_r(i256 %key) {
; CHECK: precompile r1
; CHECK: precompile.first r1
  call void @llvm.syncvm.precompile(i256 %key, i256 0)
  call void @llvm.syncvm.precompile(i256 %key, i256 1)
  ret void
}

declare i256 @llvm.syncvm.context(i256)
declare i256 @llvm.syncvm.ergsleft()
declare i256 @llvm.syncvm.sload(i256, i256)
declare void @llvm.syncvm.sstore(i256, i256, i256)
declare void @llvm.syncvm.tol1(i256, i256, i256)
declare void @llvm.syncvm.event(i256, i256, i256)
declare void @llvm.syncvm.precompile(i256, i256)
