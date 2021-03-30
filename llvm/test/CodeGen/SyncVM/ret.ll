; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

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
