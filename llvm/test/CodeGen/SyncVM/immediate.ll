; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: materialize_small_imm
define i256 @materialize_small_imm() nounwind {
  ; CHECK: add 65535, r0, r1
  ret i256 65535
}

; CHECK-LABEL: materialize_big_imm
define i256 @materialize_big_imm() nounwind {
  ; CHECK: add code[CPI{{[0-9]}}_0], r0, r1
  ret i256 65536
}

; CHECK-LABEL: materialize_negative_imm
define i256 @materialize_negative_imm(i256 %par) nounwind {
  ; CHECK: add code[CPI{{[0-9]}}_{{[0-9]}}], r0, r1
  ret i256 -1
}

