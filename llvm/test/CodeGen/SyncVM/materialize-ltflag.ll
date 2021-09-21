; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

define void @foo() {
; CHECK:   callfs r2
; CHECK:   jlt .LBB0_2, .LBB0_1
; CHECK: .LBB0_1:
; CHECK:   sfll #0, r2, r2
; CHECK:   sflh #0, r2, r2
; CHECK:   j .LBB0_3, .LBB0_3
; CHECK: .LBB0_2:
; CHECK:   sfll #340282366920938463463374607431768211455, r2, r2
; CHECK:   sflh #340282366920938463463374607431768211455, r2, r2
; CHECK: .LBB0_3:
; CHECK:   mov r2, 1
; CHECK:   ret
  tail call void @llvm.syncvm.staticcall(i256 42)
  %1 = tail call i256 @llvm.syncvm.ltflag()
  store i256 %1, i256 addrspace(1)* inttoptr (i256 32 to i256 addrspace(1)*), align 32
  ret void
}

declare void @llvm.syncvm.staticcall(i256)
declare i256 @llvm.syncvm.ltflag()
