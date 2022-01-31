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

; CHECK-LABEL: sload_ir
define i256 @sload_ir() {
; CHECK-DAG: sload 42, r{{[0-9]+}}
; CHECK-DAG: sload.first 43, r{{[0-9]+}}
  %1 = call i256 @llvm.syncvm.sload(i256 42, i256 0)
  %2 = call i256 @llvm.syncvm.sload(i256 43, i256 1)
  %3 = add i256 %2, %1
  ret i256 %3
}

; CHECK-LABEL: sload_cr
define i256 @sload_cr() {
; CHECK-DAG: sload @val[0], r{{[0-9]+}}
; CHECK-DAG: sload.first @val2[0], r{{[0-9]+}}
  %addr1 = load i256, i256 addrspace(4)* @val
  %addr2 = load i256, i256 addrspace(4)* @val2
  %1 = call i256 @llvm.syncvm.sload(i256 %addr1, i256 0)
  %2 = call i256 @llvm.syncvm.sload(i256 %addr2, i256 1)
  %3 = add i256 %2, %1
  ret i256 %3
}

; CHECK-LABEL: sload_sr
define i256 @sload_sr() {
; CHECK-DAG: sload stack-[2], r{{[0-9]+}}
; CHECK-DAG: sload.first stack-[1], r{{[0-9]+}}
  %ptr1 = alloca i256
  %ptr2 = alloca i256
  %addr1 = load i256, i256* %ptr1
  %addr2 = load i256, i256* %ptr2
  %1 = call i256 @llvm.syncvm.sload(i256 %addr1, i256 0)
  %2 = call i256 @llvm.syncvm.sload(i256 %addr2, i256 1)
  %3 = add i256 %2, %1
  ret i256 %3
}

; CHECK-LABEL: sload_rs
define void @sload_rs(i256 %val) {
  %res1 = alloca i256
  %res2 = alloca i256
; CHECK-DAG: sload r1, stack-[2]
; CHECK-DAG: sload.first 
; TODO: CPR-447 Should be sload.first r1, stack-[1]
  %1 = call i256 @llvm.syncvm.sload(i256 %val, i256 0)
  %2 = call i256 @llvm.syncvm.sload(i256 %val, i256 1)
  store i256 %1, i256* %res1
  store i256 %2, i256* %res2
  ret void
}

; CHECK-LABEL: sload_is
define void @sload_is() {
  %res1 = alloca i256
  %res2 = alloca i256
; CHECK-DAG: sload 42, stack-[2]
; CHECK-DAG: sload.first 
; TODO: CPR-447 Should be sload.first 43, stack-[1]
  %1 = call i256 @llvm.syncvm.sload(i256 42, i256 0)
  %2 = call i256 @llvm.syncvm.sload(i256 43, i256 1)
  store i256 %1, i256* %res1
  store i256 %2, i256* %res2
  ret void
}

; CHECK-LABEL: sload_cs
define void @sload_cs() {
  %res1 = alloca i256
  %res2 = alloca i256
; CHECK-DAG: sload 42, stack-[2]
; CHECK-DAG: sload.first 
; TODO: CPR-447 Should be sload.first 43, stack-[1]
  %1 = call i256 @llvm.syncvm.sload(i256 42, i256 0)
  %2 = call i256 @llvm.syncvm.sload(i256 43, i256 1)
  store i256 %1, i256* %res1
  store i256 %2, i256* %res2
  ret void
}

; CHECK-LABEL: sstore_r
define void @sstore_r(i256 %key, i256 %val) {
; CHECK: sstore r1, r2
; CHECK: sstore.first r1, r2
  call void @llvm.syncvm.sstore(i256 %key, i256 %val, i256 0)
  call void @llvm.syncvm.sstore(i256 %key, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: sstore_i
define void @sstore_i(i256 %key, i256 %val) {
; CHECK: sstore 42, r2
; CHECK: sstore.first 43, r2
  call void @llvm.syncvm.sstore(i256 42, i256 %val, i256 0)
  call void @llvm.syncvm.sstore(i256 43, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: sstore_c
define void @sstore_c(i256 %key, i256 %val) {
; CHECK: sstore @val[0], r2
; CHECK: sstore.first
; TODO: CPR-447 should be sstore.first @val2[0], r2
  %addr1 = load i256, i256 addrspace(4)* @val
  %addr2 = load i256, i256 addrspace(4)* @val2
  call void @llvm.syncvm.sstore(i256 %addr1, i256 %val, i256 0)
  call void @llvm.syncvm.sstore(i256 %addr2, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: sstore_s
define void @sstore_s(i256 %key, i256 %val) {
  %ptr1 = alloca i256
  %ptr2 = alloca i256
  %addr1 = load i256, i256* %ptr1
  %addr2 = load i256, i256* %ptr2
; CHECK: sstore stack-[2], r2
; CHECK: sstore.first
; TODO: CPR-447 should be sstore.first stack-[1], r2
  call void @llvm.syncvm.sstore(i256 %addr1, i256 %val, i256 0)
  call void @llvm.syncvm.sstore(i256 %addr2, i256 %val, i256 1)
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

; CHECK-LABEL: tol1_i
define void @tol1_i(i256 %key, i256 %val) {
; CHECK: to_l1 42, r2
; CHECK: to_l1.first 43, r2
  call void @llvm.syncvm.tol1(i256 42, i256 %val, i256 0)
  call void @llvm.syncvm.tol1(i256 43, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: tol1_c
define void @tol1_c(i256 %key, i256 %val) {
; CHECK: to_l1 @val[0], r2
; CHECK: to_l1.first
; TODO: CPR-447 should be tol1.first @val2[0], r2
  %addr1 = load i256, i256 addrspace(4)* @val
  %addr2 = load i256, i256 addrspace(4)* @val2
  call void @llvm.syncvm.tol1(i256 %addr1, i256 %val, i256 0)
  call void @llvm.syncvm.tol1(i256 %addr2, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: tol1_s
define void @tol1_s(i256 %key, i256 %val) {
  %ptr1 = alloca i256
  %ptr2 = alloca i256
  %addr1 = load i256, i256* %ptr1
  %addr2 = load i256, i256* %ptr2
; CHECK: to_l1 stack-[2], r2
; CHECK: to_l1.first
; TODO: CPR-447 should be tol1.first stack-[1], r2
  call void @llvm.syncvm.tol1(i256 %addr1, i256 %val, i256 0)
  call void @llvm.syncvm.tol1(i256 %addr2, i256 %val, i256 1)
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

; CHECK-LABEL: event_i
define void @event_i(i256 %key, i256 %val) {
; CHECK: event 42, r2
; CHECK: event.first 43, r2
  call void @llvm.syncvm.event(i256 42, i256 %val, i256 0)
  call void @llvm.syncvm.event(i256 43, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: event_c
define void @event_c(i256 %key, i256 %val) {
; CHECK: event @val[0], r2
; CHECK: event.first
; TODO: CPR-447 should be event.first @val2[0], r2
  %addr1 = load i256, i256 addrspace(4)* @val
  %addr2 = load i256, i256 addrspace(4)* @val2
  call void @llvm.syncvm.event(i256 %addr1, i256 %val, i256 0)
  call void @llvm.syncvm.event(i256 %addr2, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: event_s
define void @event_s(i256 %key, i256 %val) {
  %ptr1 = alloca i256
  %ptr2 = alloca i256
  %addr1 = load i256, i256* %ptr1
  %addr2 = load i256, i256* %ptr2
; CHECK: event stack-[2], r2
; CHECK: event.first
; TODO: CPR-447 should be event.first stack-[1], r2
  call void @llvm.syncvm.event(i256 %addr1, i256 %val, i256 0)
  call void @llvm.syncvm.event(i256 %addr2, i256 %val, i256 1)
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

; CHECK-LABEL: precompile_i
define void @precompile_i() {
; CHECK: precompile 42
; CHECK: precompile.first 43
  call void @llvm.syncvm.precompile(i256 42, i256 0)
  call void @llvm.syncvm.precompile(i256 43, i256 1)
  ret void
}

; CHECK-LABEL: precompile_c
define void @precompile_c() {
; CHECK: precompile @val[0]
; CHECK: precompile.first
; TODO: CPR-447 should be precompile.first @val2[0]
  %addr1 = load i256, i256 addrspace(4)* @val
  %addr2 = load i256, i256 addrspace(4)* @val2
  call void @llvm.syncvm.precompile(i256 %addr1, i256 0)
  call void @llvm.syncvm.precompile(i256 %addr2, i256 1)
  ret void
}

; CHECK-LABEL: precompile_s
define void @precompile_s(i256 %key, i256 %val) {
  %ptr1 = alloca i256
  %ptr2 = alloca i256
  %addr1 = load i256, i256* %ptr1
  %addr2 = load i256, i256* %ptr2
; CHECK: precompile stack-[2]
; CHECK: precompile.first
; TODO: CPR-447 should be precompile.first stack-[1]
  call void @llvm.syncvm.precompile(i256 %addr1, i256 0)
  call void @llvm.syncvm.precompile(i256 %addr2, i256 1)
  ret void
}

declare i256 @llvm.syncvm.context(i256)
declare i256 @llvm.syncvm.ergsleft()
declare i256 @llvm.syncvm.sload(i256, i256)
declare void @llvm.syncvm.sstore(i256, i256, i256)
declare void @llvm.syncvm.tol1(i256, i256, i256)
declare void @llvm.syncvm.event(i256, i256, i256)
declare void @llvm.syncvm.precompile(i256, i256)
