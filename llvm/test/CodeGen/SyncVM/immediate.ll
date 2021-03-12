; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:16:8-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: add_reg_imm
define i64 @add_reg_imm(i64 %a) nounwind {
; CHECK: cnst 42, r2
  %1 = add i64 %a, 42
  ret i64 %1
}

; CHECK-LABEL: add_imm_reg
define i64 @add_imm_reg(i64 %a) nounwind {
; CHECK: cnst 42, r2
  %1 = add i64 42, %a
  ret i64 %1
}

; CHECK-LABEL: add_imm_imm
define i64 @add_imm_imm(i64 %a) nounwind {
; CHECK: cnst 43, r1
  %1 = add i64 42, 1
  ret i64 %1
}
