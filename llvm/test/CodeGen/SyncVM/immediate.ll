; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:16:8-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: addi256
define i64 @cnst64(i64 %op1) nounwind {
; CHECK: cnst 42 2
  %1 = add i64 %op1, 42
}
