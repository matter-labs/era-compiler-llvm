; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: contextr
define i256 @contextr() {
; CHECK: ctx #1, r1
  %1 = call i256 @llvm.syncvm.context(i256 0)
  %2 = call i256 @llvm.syncvm.context(i256 1)
  %3 = call i256 @llvm.syncvm.context(i256 2)
  %4 = call i256 @llvm.syncvm.context(i256 3)
  %5 = call i256 @llvm.syncvm.context(i256 4)
  %6 = call i256 @llvm.syncvm.context(i256 5)
  %7 = call i256 @llvm.syncvm.ergsleft()
  ret i256 %1
}

declare i256 @llvm.syncvm.context(i256)
declare i256 @llvm.syncvm.ergsleft()
