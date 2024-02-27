; RUN: llc -O3 < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

define i256 @test_large_imm_no_fold1(i256 %a, i1 %cond) {
; CHECK-LABEL: test_large_imm_no_fold1
; CHECK:       add @CPI0_0[0], r1, r3
; CHECK-NEXT:  sub.s! @CPI0_1[0], r1, r4
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, 26959946660873538059280334323183841250350249843923952699046031785980
  %cmp = icmp ult i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

define i256 @test_large_imm_no_fold2(i256 %a, i1 %cond) {
; CHECK-LABEL: test_large_imm_no_fold2
; CHECK:       add @CPI1_0[0], r1, r3
; CHECK-NEXT:  sub.s! @CPI1_1[0], r1, r4
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, -26959946660873538059280334323183841250350249843923952699046031785980
  %cmp = icmp ult i256 %a, 26959946660873538059280334323183841250350249843923952699046031785985
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

define i256 @test_small_imm_no_fold(i256 %a, i1 %cond) {
; CHECK-LABEL: test_small_imm_no_fold
; CHECK:       sub.s 5, r1, r3
; CHECK-NEXT:  sub.s! 10, r1, r4
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, -5
  %cmp = icmp ult i256 %a, 10
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

define i256 @test_large_imm_ult1(i256 %a, i1 %cond) {
; CHECK-LABEL: test_large_imm_ult1
; CHECK:       sub.s! @CPI3_1[0], r1, r3
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, 26959946660873538059280334323183841250350249843923952699046031785985
  %cmp = icmp ult i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

define i256 @test_large_imm_ult2(i256 %a, i1 %cond) {
; CHECK-LABEL: test_large_imm_ult2
; CHECK:       sub.s! @CPI4_1[0], r1, r3
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %cmp = icmp ult i256 %a, 26959946660873538059280334323183841250350249843923952699046031785985
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

; TODO: CPR-1543 This can be folded.
define i256 @test_large_imm_ule1(i256 %a, i1 %cond) {
; CHECK-LABEL: test_large_imm_ule1
; CHECK:       add @CPI5_0[0], r1, r3
; CHECK-NEXT:  sub.s! @CPI5_1[0], r1, r4
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, 26959946660873538059280334323183841250350249843923952699046031785985
  %cmp = icmp ule i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

; TODO: CPR-1543 This can be folded.
define i256 @test_large_imm_ule2(i256 %a, i1 %cond) {
; CHECK-LABEL: test_large_imm_ule2
; CHECK:       add @CPI6_0[0], r1, r3
; CHECK-NEXT:  sub.s! @CPI6_1[0], r1, r4
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %cmp = icmp ule i256 %a, 26959946660873538059280334323183841250350249843923952699046031785985
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

; TODO: CPR-1543 This can be folded.
define i256 @test_large_imm_uge1(i256 %a, i1 %cond) {
; CHECK-LABEL: test_large_imm_uge1
; CHECK:       add @CPI7_0[0], r1, r3
; CHECK-NEXT:  sub.s! @CPI7_1[0], r1, r4
; CHECK-NEXT:  add.gt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, 26959946660873538059280334323183841250350249843923952699046031785985
  %cmp = icmp uge i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

; TODO: CPR-1543 This can be folded.
define i256 @test_large_imm_uge2(i256 %a, i1 %cond) {
; CHECK-LABEL: test_large_imm_uge2
; CHECK:       add @CPI8_0[0], r1, r3
; CHECK-NEXT:  sub.s! @CPI8_1[0], r1, r4
; CHECK-NEXT:  add.gt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, -26959946660873538059280334323183841250350249843923952699046031785985
  %cmp = icmp uge i256 %a, 26959946660873538059280334323183841250350249843923952699046031785985
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

define i256 @test_small_imm_ult(i256 %a, i1 %cond) {
; CHECK-LABEL: test_small_imm_ult
; CHECK:       sub.s! 5, r1, r3
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, -5
  %cmp = icmp ult i256 %a, 5
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

; TODO: CPR-1543 This can be folded.
define i256 @test_small_imm_ule(i256 %a, i1 %cond) {
; CHECK-LABEL: test_small_imm_ule
; CHECK:       sub.s 5, r1, r3
; CHECK-NEXT:  sub.s! 6, r1, r4
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, -5
  %cmp = icmp ule i256 %a, 5
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

; TODO: CPR-1543 This can be folded.
define i256 @test_small_imm_uge(i256 %a, i1 %cond) {
; CHECK-LABEL: test_small_imm_uge
; CHECK:       sub.s 5, r1, r3
; CHECK-NEXT:  sub.s! 4, r1, r4
; CHECK-NEXT:  add.gt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %add = add i256 %a, -5
  %cmp = icmp uge i256 %a, 5
  %select1 = select i1 %cmp, i256 %add, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %add
  ret i256 %select2
}

define i256 @test_reg1(i256 %a, i256 %b, i1 %cond) {
; CHECK-LABEL: test_reg1
; CHECK:       sub! r1, r2, r2
; CHECK-NEXT:  add.lt r2, r0, r1
; CHECK-NEXT:  sub! r3, r0, r3
; CHECK-NEXT:  add.eq r2, r0, r1
; CHECK-NEXT:  ret
  %sub = sub i256 %a, %b
  %cmp = icmp ult i256 %a, %b
  %select1 = select i1 %cmp, i256 %sub, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %sub
  ret i256 %select2
}

define i256 @test_reg2(i256 %a, i256 %b, i1 %cond) {
; CHECK-LABEL: test_reg2
; CHECK:       sub! r2, r1, r2
; CHECK-NEXT:  add.lt r2, r0, r1
; CHECK-NEXT:  sub! r3, r0, r3
; CHECK-NEXT:  add.eq r2, r0, r1
; CHECK-NEXT:  ret
  %sub = sub i256 %b, %a
  %cmp = icmp ult i256 %b, %a
  %select1 = select i1 %cmp, i256 %sub, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %sub
  ret i256 %select2
}

define i256 @test_stack1(i256 %a, i1 %cond) {
; CHECK-LABEL: test_stack1
; CHECK:       sub.s! stack-[1], r1, r3
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %bptr = alloca i256
  %b = load i256, i256* %bptr
  %sub = sub i256 %a, %b
  %cmp = icmp ult i256 %a, %b
  %select1 = select i1 %cmp, i256 %sub, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %sub
  ret i256 %select2
}

define i256 @test_stack2(i256 %a, i1 %cond) {
; CHECK-LABEL: test_stack2
; CHECK:       sub! stack-[1], r1, r3
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %bptr = alloca i256
  %b = load i256, i256* %bptr
  %sub = sub i256 %b, %a
  %cmp = icmp ult i256 %b, %a
  %select1 = select i1 %cmp, i256 %sub, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %sub
  ret i256 %select2
}

define i256 @test_code1(i256 %a, i1 %cond) {
; CHECK-LABEL: test_code1
; CHECK:       sub.s! @val[0], r1, r3
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %b = load i256, i256 addrspace(4)* @val
  %sub = sub i256 %a, %b
  %cmp = icmp ult i256 %a, %b
  %select1 = select i1 %cmp, i256 %sub, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %sub
  ret i256 %select2
}

define i256 @test_code2(i256 %a, i1 %cond) {
; CHECK-LABEL: test_code2
; CHECK:       sub  @val[0], r1, r3
; CHECK-NEXT:  sub!	@val[0], r1, r4
; CHECK-NEXT:  add.lt r3, r0, r1
; CHECK-NEXT:  sub! r2, r0, r2
; CHECK-NEXT:  add.eq r3, r0, r1
; CHECK-NEXT:  ret
  %b = load i256, i256 addrspace(4)* @val
  %sub = sub i256 %b, %a
  %cmp = icmp ult i256 %b, %a
  %select1 = select i1 %cmp, i256 %sub, i256 %a
  %select2 = select i1 %cond, i256 %select1, i256 %sub
  ret i256 %select2
}
