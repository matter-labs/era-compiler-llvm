; RUN: opt -passes=eravm-cse -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test_addmod_dce(i256 %arg1, i256 %arg2, i256 %arg3) {
; CHECK-LABEL: @test_addmod_dce(
; CHECK-NOT:     call i256 @__addmod
;
entry:
  %val1 = call i256 @__addmod(i256 %arg1, i256 %arg2, i256 %arg3)
  ret i256 0
}

define i256 @test_clz_dce(i256 %arg1) {
; CHECK-LABEL: @test_clz_dce(
; CHECK-NOT:     call i256 @__clz
;
entry:
  %val1 = call i256 @__clz(i256 %arg1)
  ret i256 0
}

define i256 @test_ulongrem_dce(i256 %arg1, i256 %arg2, i256 %arg3) {
; CHECK-LABEL: @test_ulongrem_dce(
; CHECK-NOT:     call i256 @__ulongrem
;
entry:
  %val1 = call i256 @__ulongrem(i256 %arg1, i256 %arg2, i256 %arg3)
  ret i256 0
}

define i256 @test_mulmod_dce(i256 %arg1, i256 %arg2, i256 %arg3) {
; CHECK-LABEL: @test_mulmod_dce(
; CHECK-NOT:     call i256 @__mulmod
;
entry:
  %val1 = call i256 @__mulmod(i256 %arg1, i256 %arg2, i256 %arg3)
  ret i256 0
}

define i256 @test_signextend_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_signextend_dce(
; CHECK-NOT:     call i256 @__signextend
;
entry:
  %val1 = call i256 @__signextend(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_exp_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_exp_dce(
; CHECK-NOT:     call i256 @__exp
;
entry:
  %val1 = call i256 @__exp(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_exp_pow2_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_exp_pow2_dce(
; CHECK-NOT:     call i256 @__exp_pow2
;
entry:
  %val1 = call i256 @__exp_pow2(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_div_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_div_dce(
; CHECK-NOT:     call i256 @__div
;
entry:
  %val1 = call i256 @__div(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_sdiv_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_sdiv_dce(
; CHECK-NOT:     call i256 @__sdiv
;
entry:
  %val1 = call i256 @__sdiv(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_mod_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_mod_dce(
; CHECK-NOT:     call i256 @__mod
;
entry:
  %val1 = call i256 @__mod(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_smod_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_smod_dce(
; CHECK-NOT:     call i256 @__smod
;
entry:
  %val1 = call i256 @__smod(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_byte_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_byte_dce(
; CHECK-NOT:     call i256 @__byte
;
entry:
  %val1 = call i256 @__byte(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_shl_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_shl_dce(
; CHECK-NOT:     call i256 @__shl
;
entry:
  %val1 = call i256 @__shl(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_shr_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_shr_dce(
; CHECK-NOT:     call i256 @__shr
;
entry:
  %val1 = call i256 @__shr(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_sar_dce(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_sar_dce(
; CHECK-NOT:     call i256 @__sar
;
entry:
  %val1 = call i256 @__sar(i256 %arg1, i256 %arg2)
  ret i256 0
}

define i256 @test_addmod_elim(i256 %arg1, i256 %arg2, i256 %arg3) {
; CHECK-LABEL: @test_addmod_elim(
; CHECK:         call i256 @__addmod
; CHECK-NOT:     call i256 @__addmod
;
entry:
  %val1 = call i256 @__addmod(i256 %arg1, i256 %arg2, i256 %arg3)
  %val2 = call i256 @__addmod(i256 %arg1, i256 %arg2, i256 %arg3)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_clz_elim(i256 %arg1) {
; CHECK-LABEL: @test_clz_elim(
; CHECK:         call i256 @__clz
; CHECK-NOT:     call i256 @__clz
;
entry:
  %val1 = call i256 @__clz(i256 %arg1)
  %val2 = call i256 @__clz(i256 %arg1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_ulongrem_elim(i256 %arg1, i256 %arg2, i256 %arg3) {
; CHECK-LABEL: @test_ulongrem_elim(
; CHECK:         call i256 @__ulongrem
; CHECK-NOT:     call i256 @__ulongrem
;
entry:
  %val1 = call i256 @__ulongrem(i256 %arg1, i256 %arg2, i256 %arg3)
  %val2 = call i256 @__ulongrem(i256 %arg1, i256 %arg2, i256 %arg3)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_mulmod_elim(i256 %arg1, i256 %arg2, i256 %arg3) {
; CHECK-LABEL: @test_mulmod_elim(
; CHECK:         call i256 @__mulmod
; CHECK-NOT:     call i256 @__mulmod
;
entry:
  %val1 = call i256 @__mulmod(i256 %arg1, i256 %arg2, i256 %arg3)
  %val2 = call i256 @__mulmod(i256 %arg1, i256 %arg2, i256 %arg3)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_signextend_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_signextend_elim(
; CHECK:         call i256 @__signextend
; CHECK-NOT:     call i256 @__signextend
;
entry:
  %val1 = call i256 @__signextend(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__signextend(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_exp_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_exp_elim(
; CHECK:         call i256 @__exp
; CHECK-NOT:     call i256 @__exp
;
entry:
  %val1 = call i256 @__exp(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__exp(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_exp_pow2_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_exp_pow2_elim(
; CHECK:         call i256 @__exp_pow2
; CHECK-NOT:     call i256 @__exp_pow2
;
entry:
  %val1 = call i256 @__exp_pow2(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__exp_pow2(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_div_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_div_elim(
; CHECK:         call i256 @__div
; CHECK-NOT:     call i256 @__div
;
entry:
  %val1 = call i256 @__div(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__div(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sdiv_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_sdiv_elim(
; CHECK:         call i256 @__sdiv
; CHECK-NOT:     call i256 @__sdiv
;
entry:
  %val1 = call i256 @__sdiv(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__sdiv(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_mod_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_mod_elim(
; CHECK:         call i256 @__mod
; CHECK-NOT:     call i256 @__mod
;
entry:
  %val1 = call i256 @__mod(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__mod(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_smod_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_smod_elim(
; CHECK:         call i256 @__smod
; CHECK-NOT:     call i256 @__smod
;
entry:
  %val1 = call i256 @__smod(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__smod(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_byte_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_byte_elim(
; CHECK:         call i256 @__byte
; CHECK-NOT:     call i256 @__byte
;
entry:
  %val1 = call i256 @__byte(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__byte(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_shl_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_shl_elim(
; CHECK:         call i256 @__shl
; CHECK-NOT:     call i256 @__shl
;
entry:
  %val1 = call i256 @__shl(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__shl(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_shr_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_shr_elim(
; CHECK:         call i256 @__shr
; CHECK-NOT:     call i256 @__shr
;
entry:
  %val1 = call i256 @__shr(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__shr(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sar_elim(i256 %arg1, i256 %arg2) {
; CHECK-LABEL: @test_sar_elim(
; CHECK:         call i256 @__sar
; CHECK-NOT:     call i256 @__sar
;
entry:
  %val1 = call i256 @__sar(i256 %arg1, i256 %arg2)
  %val2 = call i256 @__sar(i256 %arg1, i256 %arg2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

declare i256 @__addmod(i256, i256, i256) #0
declare i256 @__clz(i256) #0
declare i256 @__ulongrem(i256, i256, i256) #0
declare i256 @__mulmod(i256, i256, i256) #0
declare i256 @__signextend(i256, i256) #0
declare i256 @__exp(i256, i256) #0
declare i256 @__exp_pow2(i256, i256) #0
declare i256 @__div(i256, i256) #0
declare i256 @__sdiv(i256, i256) #0
declare i256 @__mod(i256, i256) #0
declare i256 @__smod(i256, i256) #0
declare i256 @__byte(i256, i256) #0
declare i256 @__shl(i256, i256) #0
declare i256 @__shr(i256, i256) #0
declare i256 @__sar(i256, i256) #0
attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
