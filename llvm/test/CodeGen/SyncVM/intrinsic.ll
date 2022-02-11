; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

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

declare i256 @llvm.syncvm.context(i256)
declare i256 @llvm.syncvm.ergsleft()
