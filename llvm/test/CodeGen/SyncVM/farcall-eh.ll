; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: farcallrc
define i256 @farcallrc() nounwind {
; CHECK: call __farcall
  %1 = call i256 @llvm.syncvm.farcall.rc(i256 256)
  ret i256 %1
}

; CHECK-LABEL: __farcall
; CHECK: callf r1
; CHECK-NEXT: jlt

declare i256 @llvm.syncvm.farcall.rc(i256)
