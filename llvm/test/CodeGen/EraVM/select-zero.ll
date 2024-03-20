; RUN: llc --disable-eravm-scalar-opt-passes < %s | FileCheck %s

target triple = "eravm"

; CHECK-LABEL: @select_zero_1
define i256 @select_zero_1(i256 %0, i256 %1) {
; CHECK:       sub!   r1, r2, r3
; CHECK-NEXT:  add    r1, r2, r1
; CHECK-NEXT:  add.eq 1,  r1, r1
entry:
  %sum = add i256 %0, %1
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 1, i256 0
  %ret = add i256 %sum, %sel
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_2
define i256 @select_zero_2(i256 %0, i256 %1) {
; CHECK:       sub!   r1, r2, r3
; CHECK-NEXT:  add    r1, r2, r1
; CHECK-NEXT:  add.ne 1,  r1, r1
entry:
  %sum = add i256 %0, %1
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 0, i256 1
  %ret = add i256 %sum, %sel
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_3
define i256 @select_zero_3(i256 %0, i256 %1) {
; CHECK:       add    r1, r2, r3
; CHECK-NEXT:  sub!   r1, r2, r1
; CHECK-NEXT:  add.eq 1,  r3, r3
; CHECK-NEXT:  add    r3, r0, r1
entry:
  %sum = add i256 %0, %1
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 1, i256 0
  %ret = add i256 %sel, %sum
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_4
define i256 @select_zero_4(i256 %0, i256 %1) {
; CHECK:       add    r1, r2, r3
; CHECK-NEXT:  sub!   r1, r2, r1
; CHECK-NEXT:  add.ne 1,  r3, r3
; CHECK-NEXT:  add    r3, r0, r1
entry:
  %sum = add i256 %0, %1
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 0, i256 1
  %ret = add i256 %sel, %sum
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_5
define i256 @select_zero_5(i256 %0, i256 %1, i256 %2, i256 %3) {
; CHECK:       sub!   r3, r4, r3
; CHECK-NEXT:  add    0,  r0, r3
; CHECK-NEXT:  add.ne 1,  r0, r3
; CHECK-NEXT:  sub!   r1, r2, r1
; CHECK-NEXT:  add.ne 1,  r3, r3
; CHECK-NEXT:  add    r3, r0, r1
entry:
  %cmp1 = icmp eq i256 %0, %1
  %sel1 = select i1 %cmp1, i256 0, i256 1
  %cmp2 = icmp eq i256 %2, %3
  %sel2 = select i1 %cmp2, i256 0, i256 1
  %ret = add i256 %sel1, %sel2
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_or_1
define i256 @select_zero_or_1(i256 %0, i256 %1, i256 %2) {
; CHECK:       sub!   r1, r2, r1
; CHECK-NEXT:  or.ne  1,  r3, r3
; CHECK-NEXT:  add    r3, r0, r1
entry:
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 0, i256 1
  %ret = or i256 %2, %sel
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_or_2
define i256 @select_zero_or_2(i256 %0, i256 %1, i256 %2) {
; CHECK:       sub!   r1, r2, r1
; CHECK-NEXT:  or.ne  1,  r3, r3
; CHECK-NEXT:  add    r3, r0, r1
entry:
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 0, i256 1
  %ret = or i256 %sel, %2
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_sub
define i256 @select_zero_sub(i256 %0, i256 %1, i256 %2) {
; CHECK:       sub!      r1, r2, r1
; CHECK-NEXT:  sub.s.ne  1,  r3, r3
; CHECK-NEXT:  add       r3, r0, r1
entry:
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 0, i256 1
  %ret = sub i256 %2, %sel
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_sub_not
define i256 @select_zero_sub_not(i256 %0, i256 %1, i256 %2) {
; CHECK:       sub!      r1, r2, r1
; CHECK-NEXT:  add       0,  r0, r1
; CHECK-NEXT:  add.ne    1,  r0, r1
; CHECK-NEXT:  sub       r1, r3, r1
entry:
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 0, i256 1
  %ret = sub i256 %sel, %2
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_shl
define i256 @select_zero_shl(i256 %0, i256 %1, i256 %2) {
; CHECK:       sub!      r1, r2, r1
; CHECK-NEXT:  shl.s.ne  1,  r3, r3
; CHECK-NEXT:  add       r3, r0, r1
entry:
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 0, i256 1
  %ret = shl i256 %2, %sel
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_shl_not
define i256 @select_zero_shl_not(i256 %0, i256 %1, i256 %2) {
; CHECK:       sub!      r1, r2, r1
; CHECK-NEXT:  add       0,  r0, r1
; CHECK-NEXT:  add.ne    1,  r0, r1
; CHECK-NEXT:  shl       r1, r3, r1
entry:
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 0, i256 1
  %ret = shl i256 %sel, %2
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_shr
define i256 @select_zero_shr(i256 %0, i256 %1, i256 %2) {
; CHECK:       sub!      r1, r2, r1
; CHECK-NEXT:  shr.s.ne  1,  r3, r3
; CHECK-NEXT:  add       r3, r0, r1
entry:
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 0, i256 1
  %ret = lshr i256 %2, %sel
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_shr_not
define i256 @select_zero_shr_not(i256 %0, i256 %1, i256 %2) {
; CHECK:       sub!      r1, r2, r1
; CHECK-NEXT:  add       0,  r0, r1
; CHECK-NEXT:  add.ne    1,  r0, r1
; CHECK-NEXT:  shr       r1, r3, r1
entry:
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 0, i256 1
  %ret = lshr i256 %sel, %2
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_add_stack
define i256 @select_zero_add_stack(i256 %0, i256 %1) {
; CHECK:       nop    stack+=[1 + r0]
; CHECK-NEXT:  sub!   r1, r2, r1
; CHECK-NEXT:  add.eq stack-[1],  r2, r2
; CHECK-NEXT:  add    r2, r0, r1
entry:
  %ptr = alloca i256
  %val = load i256, i256* %ptr
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 %val, i256 0
  %ret = add i256 %1, %sel
  ret i256 %ret
}

@val = addrspace(4) global i256 42
; CHECK-LABEL: @select_zero_add_code
define i256 @select_zero_add_code(i256 %0, i256 %1) {
; CHECK:       sub!   r1, r2, r1
; CHECK-NEXT:  add.eq  @val[0], r2, r2
; CHECK-NEXT:  add    r2, r0, r1
entry:
  %val = load i256, i256 addrspace(4)* @val
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 %val, i256 0
  %ret = add i256 %1, %sel
  ret i256 %ret
}

; CHECK-LABEL: @select_zero_not_folded_1
define i256 @select_zero_not_folded_1(i256 %0, i256 %1) {
; CHECK:       nop    stack+=[1 + r0]
; CHECK-NEXT:  add    r1, r2, r3
; CHECK-NEXT:  sub!   r1, r2, r1
; CHECK-NEXT:  add    0,  r0, r1
; CHECK-NEXT:  add.eq 1,  r0, r1
; CHECK-NEXT:  add    r1, r0, stack-[1]
; CHECK-NEXT:  add    r3, r1, r1
entry:
  %valptr = alloca i256
  %sum = add i256 %0, %1
  %cmp = icmp eq i256 %0, %1
  %sel = select i1 %cmp, i256 1, i256 0
  %ret = add i256 %sum, %sel
  store volatile i256 %sel, i256* %valptr
  ret i256 %ret
}
