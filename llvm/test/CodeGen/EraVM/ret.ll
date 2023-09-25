; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK-LABEL: ret_void
define void @ret_void() nounwind {
; CHECK: ret
  ret void
}

; CHECK-LABEL: id
define i256 @id(i256 %par) nounwind {
; CHECK: ret
  ret i256 %par
}
