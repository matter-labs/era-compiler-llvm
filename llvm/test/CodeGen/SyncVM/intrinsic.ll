; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: habs
define void @habs(i256 %in) nounwind {
; CHECK: habs r1
  call void @llvm.syncvm.habs(i256 %in)
  ret void
}

; CHECK-LABEL: habsr
define void @habsr(i256 %in) nounwind {
; CHECK: habsr r1
  call void @llvm.syncvm.habsr(i256 %in)
  ret void
}

; CHECK-LABEL: hout
define i256 @hout() nounwind {
; CHECK: hout r1
  %1 = call i256 @llvm.syncvm.hout()
  ret i256 %1
}

declare void @llvm.syncvm.habs(i256 %in)
declare void @llvm.syncvm.habsr(i256 %in)
declare i256 @llvm.syncvm.hout()
