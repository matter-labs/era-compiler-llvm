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
  ; CHECK: add @CPI{{[0-9]}}_0[0], r0, r1
  ret i256 65536
}

; CHECK-LABEL: materialize_negative_imm
define i256 @materialize_negative_imm(i256 %par) nounwind {
  ; CHECK: add @CPI{{[0-9]}}_{{[0-9]}}[0], r0, r1
  ret i256 -1
}

; CHECK-LABEL: materialize_smallimm_in_operation
define i256 @materialize_smallimm_in_operation(i256 %par) nounwind {
  ; CHECK: add 42, r1, r1
  %res = add i256 %par, 42
  ret i256 %res
}

; CHECK-LABEL: materialize_bigimm_in_operation
define i256 @materialize_bigimm_in_operation(i256 %par) nounwind {
  ; CHECK: add @CPI{{[0-9]}}_{{[0-9]}}[0], r1, r1
  %res = add i256 %par, -42
  ret i256 %res
}

; CHECK-LABEL: materialize_bigimm_in_operation_2
define i256 @materialize_bigimm_in_operation_2(i256 %par) nounwind {
  ; CHECK: add @CPI{{[0-9]}}_{{[0-9]}}[0], r1, r1
  %res = add i256 -42, %par
  ret i256 %res
}

; CHECK-LABEL: materialize_bigimm_in_and_operation
define i256 @materialize_bigimm_in_and_operation(i256 %par) nounwind {
  ; CHECK: and @CPI{{[0-9]}}_{{[0-9]}}[0], r1, r1
  %res = and i256 %par, -42
  ret i256 %res
}

; CHECK-LABEL: materialize_bigimm_in_xor_operation
define i256 @materialize_bigimm_in_xor_operation(i256 %par) nounwind {
  ; CHECK: xor @CPI{{[0-9]}}_{{[0-9]}}[0], r1, r1
  %res = xor i256 -42, %par
  ret i256 %res
}

; CHECK-LABEL: materialize_bigimm_in_sub_operation
define i256 @materialize_bigimm_in_sub_operation(i256 %par) nounwind {
  ; CHECK: sub.s @CPI{{[0-9]}}_{{[0-9]}}[0], r1, r1
  %res = sub i256 %par, -42
  ret i256 %res
}

; CHECK-LABEL: materialize_bigimm_in_sub_operation_2
define i256 @materialize_bigimm_in_sub_operation_2(i256 %par) nounwind {
  ; CHECK: sub @CPI{{[0-9]}}_{{[0-9]}}[0], r1, r1
  %res = sub i256 -42, %par
  ret i256 %res
}
