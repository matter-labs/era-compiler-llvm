; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:16:8-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: cnst64
define i64 @cnst64(i64 %a) nounwind {
; CHECK: cnst 67, r1
  %1 = add i64 25, 42
  ret i64 %1
}
