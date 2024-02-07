; RUN: llc -O3 < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

define i256 @test_large_imm1(i256 %a) {
; CHECK-LABEL: test_large_imm1
; CHECK:       sub.s! @CPI0_1[0], r1, r2
; CHECK-NEXT:  add.lt @CPI0_0[0], r1, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, 26959946660873538059280334323183841250350249843923952699046031785980
  %cmp = icmp ult i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %select = select i1 %cmp, i256 %add, i256 %a
  ret i256 %select
}

define i256 @test_large_imm2(i256 %a) {
; CHECK-LABEL: test_large_imm2
; CHECK:       sub.s! @CPI1_1[0], r1, r2
; CHECK-NEXT:  add.lt @CPI1_0[0], r1, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %cmp = icmp ult i256 %a, 26959946660873538059280334323183841250350249843923952699046031785980
  %select = select i1 %cmp, i256 %add, i256 %a
  ret i256 %select
}

define i256 @test_large_imm3(i256 %a) {
; CHECK-LABEL: test_large_imm3
; CHECK:       sub.s! @CPI2_1[0], r1, r2
; CHECK-NEXT:  add.ge @CPI2_0[0], r1, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, 26959946660873538059280334323183841250350249843923952699046031785980
  %cmp = icmp ult i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %select = select i1 %cmp, i256 %a, i256 %add
  ret i256 %select
}

define i256 @test_large_imm4(i256 %a) {
; CHECK-LABEL: test_large_imm4
; CHECK:       sub.s! @CPI3_1[0], r1, r2
; CHECK-NEXT:  add.ge @CPI3_0[0], r1, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %cmp = icmp ult i256 %a, 26959946660873538059280334323183841250350249843923952699046031785980
  %select = select i1 %cmp, i256 %a, i256 %add
  ret i256 %select
}

define i256 @test_small_imm1(i256 %a) {
; CHECK-LABEL: test_small_imm1
; CHECK:       sub.s 5, r0, r2
; CHECK-NEXT:  sub! r1, r2, r2
; CHECK-NEXT:  add.lt 10, r1, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, 10
  %cmp = icmp ult i256 %a, -5
  %select = select i1 %cmp, i256 %add, i256 %a
  ret i256 %select
}

define i256 @test_small_imm2(i256 %a) {
; CHECK-LABEL: test_small_imm2
; CHECK:       sub.s 5, r0, r2
; CHECK-NEXT:  sub! r1, r2, r2
; CHECK-NEXT:  add.ge 10, r1, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, 10
  %cmp = icmp ult i256 %a, -5
  %select = select i1 %cmp, i256 %a, i256 %add
  ret i256 %select
}

define i256 @test_reg1(i256 %a, i256 %b) {
; CHECK-LABEL: test_reg1
; CHECK:       sub! r1, r2, r3
; CHECK-NEXT:  add.lt r1, r2, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, %b
  %cmp = icmp ult i256 %a, %b
  %select = select i1 %cmp, i256 %add, i256 %a
  ret i256 %select
}

define i256 @test_reg2(i256 %a, i256 %b) {
; CHECK-LABEL: test_reg2
; CHECK:       sub! r1, r2, r3
; CHECK-NEXT:  add.ge r1, r2, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, %b
  %cmp = icmp ult i256 %a, %b
  %select = select i1 %cmp, i256 %a, i256 %add
  ret i256 %select
}

define i256 @test_reg3(i256 %a, i256 %b) {
; CHECK-LABEL: test_reg3
; CHECK:       sub! r1, r2, r3
; CHECK-NEXT:  add.lt r1, r2, r2
; CHECK-NEXT:  add r2, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, %b
  %cmp = icmp ult i256 %a, %b
  %select = select i1 %cmp, i256 %add, i256 %b
  ret i256 %select
}

define i256 @test_reg4(i256 %a, i256 %b) {
; CHECK-LABEL: test_reg4
; CHECK:       sub! r1, r2, r3
; CHECK-NEXT:  add.ge r1, r2, r2
; CHECK-NEXT:  add r2, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, %b
  %cmp = icmp ult i256 %a, %b
  %select = select i1 %cmp, i256 %b, i256 %add
  ret i256 %select
}

define i256 @test_stack1(i256 %a) {
; CHECK-LABEL: test_stack1
; CHECK:       sub.s! 5, r1, r2
; CHECK-NEXT:  add.lt stack-[1], r1, r1
; CHECK-NEXT:  ret
  %bptr = alloca i256
  %b = load i256, i256* %bptr
  %add = add i256 %a, %b
  %cmp = icmp ult i256 %a, 5
  %select = select i1 %cmp, i256 %add, i256 %a
  ret i256 %select
}

define i256 @test_stack2(i256 %a) {
; CHECK-LABEL: test_stack2
; CHECK:       sub.s! 5, r1, r2
; CHECK-NEXT:  add.ge stack-[1], r1, r1
; CHECK-NEXT:  ret
  %bptr = alloca i256
  %b = load i256, i256* %bptr
  %add = add i256 %a, %b
  %cmp = icmp ult i256 %a, 5
  %select = select i1 %cmp, i256 %a, i256 %add
  ret i256 %select
}

define i256 @test_code1(i256 %a) {
; CHECK-LABEL: test_code1
; CHECK:       sub.s! 5, r1, r2
; CHECK-NEXT:  add.lt @val[0], r1, r1
; CHECK-NEXT:  ret
  %b = load i256, i256 addrspace(4)* @val
  %add = add i256 %a, %b
  %cmp = icmp ult i256 %a, 5
  %select = select i1 %cmp, i256 %add, i256 %a
  ret i256 %select
}

define i256 @test_code2(i256 %a) {
; CHECK-LABEL: test_code2
; CHECK:       sub.s! 5, r1, r2
; CHECK-NEXT:  add.ge @val[0], r1, r1
; CHECK-NEXT:  ret
  %b = load i256, i256 addrspace(4)* @val
  %add = add i256 %a, %b
  %cmp = icmp ult i256 %a, 5
  %select = select i1 %cmp, i256 %a, i256 %add
  ret i256 %select
}

define i256 @test_use_in_other_bb(i256 %a, i1 %cond) {
; CHECK-LABEL: test_use_in_other_bb
; CHECK:       .BB14_2:
; CHECK-NEXT:  sub.s! @CPI14_1[0], r1, r2
; CHECK-NEXT:  add.lt @CPI14_0[0], r1, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, 26959946660873538059280334323183841250350249843923952699046031785980
  br i1 %cond, label %then, label %else

then:
  ret i256 0

else:
  %cmp = icmp ult i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %select = select i1 %cmp, i256 %add, i256 %a
  ret i256 %select
}
