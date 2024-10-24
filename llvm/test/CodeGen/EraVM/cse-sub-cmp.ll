; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py UTC_ARGS: --version 2
; RUN: llc -O3 < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @use(i256)

define i256 @test_small_imm(i256 %a) {
; CHECK-LABEL: test_small_imm:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    sub.s! 10, r1, r2
; CHECK-NEXT:    mul 10, r1, r1, r0
; CHECK-NEXT:    add.ge r2, r0, r1
; CHECK-NEXT:    ret
  %sub = sub i256 %a, 10
  %mul = mul i256 %a, 10
  %cmp = icmp ult i256 %a, 10
  %select = select i1 %cmp, i256 %mul, i256 %sub
  ret i256 %select
}

define i256 @test_large_imm(i256 %a) {
; CHECK-LABEL: test_large_imm:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    sub.s! code[@CPI1_0], r1, r2
; CHECK-NEXT:    mul code[@CPI1_0], r1, r1, r0
; CHECK-NEXT:    add.ge r2, r0, r1
; CHECK-NEXT:    ret
  %sub = sub i256 %a, 123456789
  %mul = mul i256 %a, 123456789
  %cmp = icmp ult i256 %a, 123456789
  %select = select i1 %cmp, i256 %mul, i256 %sub
  ret i256 %select
}

define i256 @test_reg(i256 %a, i256 %b) {
; CHECK-LABEL: test_reg:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    sub! r1, r2, r3
; CHECK-NEXT:    mul r1, r2, r1, r0
; CHECK-NEXT:    add.ge r3, r0, r1
; CHECK-NEXT:    ret
  %sub = sub i256 %a, %b
  %mul = mul i256 %a, %b
  %cmp = icmp ult i256 %a, %b
  %select = select i1 %cmp, i256 %mul, i256 %sub
  ret i256 %select
}

define i256 @test_in_different_bb(i256 %a, i256 %b) {
; CHECK-LABEL: test_in_different_bb:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    sub! r1, r2, r1
; CHECK-NEXT:    add.lt r0, r0, r1
; CHECK-NEXT:    ret
  %cmp = icmp ult i256 %a, %b
  br i1 %cmp, label %bb1, label %bb2

bb1:
  ret i256 0

bb2:
  %sub = sub i256 %a, %b
  ret i256 %sub
}

define i256 @test_imm_in_different_bb(i256 %a) {
; CHECK-LABEL: test_imm_in_different_bb:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    sub.s! 10, r1, r1
; CHECK-NEXT:    add.lt r0, r0, r1
; CHECK-NEXT:    ret
  %cmp = icmp ult i256 %a, 10
  br i1 %cmp, label %bb1, label %bb2

bb1:
  ret i256 0

bb2:
  %sub = sub i256 %a, 10
  ret i256 %sub
}

define i256 @test_with_call(i256 %a, i256 %b) {
; CHECK-LABEL: test_with_call:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    incsp 1
; CHECK-NEXT:    sub! r1, r2, stack-[1]
; CHECK-NEXT:    add 10, r0, r1
; CHECK-NEXT:    add.lt 15, r0, r1
; CHECK-NEXT:    call r0, @use, @DEFAULT_UNWIND
; CHECK-NEXT:    add stack-[1], r0, r1 ; 32-byte Folded Reload
; CHECK-NEXT:    ret
  %cmp = icmp ult i256 %a, %b
  %select = select i1 %cmp, i256 15, i256 10
  call void @use(i256 %select)
  %sub = sub i256 %a, %b
  ret i256 %sub
}

; Test that cmp is not eliminated, since we don't preserve flags after the call.
define i256 @test_with_call_not(i256 %a) {
; CHECK-LABEL: test_with_call_not:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    incsp 1
; CHECK-NEXT:    add r1, r0, stack-[1] ; 32-byte Folded Spill
; CHECK-NEXT:    sub.s 10, r1, r1
; CHECK-NEXT:    call r0, @use, @DEFAULT_UNWIND
; CHECK-NEXT:    add stack-[1], r0, r1 ; 32-byte Folded Reload
; CHECK-NEXT:    sub.s! 10, r1, r0
; CHECK-NEXT:    add 10, r0, r1
; CHECK-NEXT:    add.lt 15, r0, r1
; CHECK-NEXT:    ret
  %sub = sub i256 %a, 10
  call void @use(i256 %sub)
  %cmp = icmp ult i256 %a, 10
  %select = select i1 %cmp, i256 15, i256 10
  ret i256 %select
}

define i256 @test_elim_identical_cmps(i256 %a, ptr addrspace(1) %ptr) {
; CHECK-LABEL: test_elim_identical_cmps:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    sub.s! 10, r1, r0
; CHECK-NEXT:    add 10, r0, r3
; CHECK-NEXT:    add.lt 15, r0, r3
; CHECK-NEXT:    sub r3, r1, r1
; CHECK-NEXT:    stm.h r2, r1
; CHECK-NEXT:    add 15, r0, r1
; CHECK-NEXT:    add.lt 10, r0, r1
; CHECK-NEXT:    ret
  %cmp1 = icmp ult i256 %a, 10
  %select1 = select i1 %cmp1, i256 15, i256 10
  %sub = sub i256 %select1, %a
  store i256 %sub, ptr addrspace(1) %ptr
  %cmp2 = icmp ult i256 %a, 10
  %select2 = select i1 %cmp2, i256 10, i256 15
  ret i256 %select2
}

define i256 @test_dont_elim_identical_cmps(i256 %a, ptr addrspace(1) %ptr) {
; CHECK-LABEL: test_dont_elim_identical_cmps:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    incsp 1
; CHECK-NEXT:    add r1, r0, stack-[1] ; 32-byte Folded Spill
; CHECK-NEXT:    sub.s! 10, r1, r0
; CHECK-NEXT:    add 10, r0, r3
; CHECK-NEXT:    add.lt 15, r0, r3
; CHECK-NEXT:    sub r3, r1, r1
; CHECK-NEXT:    stm.h r2, r1
; CHECK-NEXT:    call r0, @use, @DEFAULT_UNWIND
; CHECK-NEXT:    add stack-[1], r0, r1 ; 32-byte Folded Reload
; CHECK-NEXT:    sub.s! 10, r1, r0
; CHECK-NEXT:    add 15, r0, r1
; CHECK-NEXT:    add.lt 10, r0, r1
; CHECK-NEXT:    ret
  %cmp1 = icmp ult i256 %a, 10
  %select1 = select i1 %cmp1, i256 15, i256 10
  %sub = sub i256 %select1, %a
  store i256 %sub, ptr addrspace(1) %ptr
  call void @use(i256 %sub)
  %cmp2 = icmp ult i256 %a, 10
  %select2 = select i1 %cmp2, i256 10, i256 15
  ret i256 %select2
}
