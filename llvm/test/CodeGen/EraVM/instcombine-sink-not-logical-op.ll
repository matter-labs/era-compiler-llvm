; RUN: opt -S -passes='default<Oz>' < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

define i256 @sink_not_into_logical_op(i256 %x, i256 %n, i256 %z) {
; CHECK-LABEL: define {{.*}}@sink_not_into_logical_op(
; CHECK-DAG:   ret i256 0
; CHECK-DAG:   store i256 167, ptr addrspace(5) null
;
entry:
  %cmp_sgt = icmp sgt i256 %x, -2
  %cmp_sgt_ext = zext i1 %cmp_sgt to i256
  %cmp_eq = icmp eq i256 %cmp_sgt_ext, 0
  %cmp_ext = zext i1 %cmp_eq to i256
  %p_init = icmp slt i256 %x, -1
  %p_init_ext = zext i1 %p_init to i256
  %div2 = lshr i256 %z, 1
  %guard = icmp ugt i256 %div2, 127
  br i1 %guard, label %exit_early, label %loop

exit_early:
  ret i256 0

loop:
  %p = phi i256 [ %p_init_ext, %entry ], [ %p_next_ext, %latch ]
  %not1 = icmp eq i256 %p, 0
  %not1_ext = zext i1 %not1 to i256
  %or = or i256 %cmp_ext, %not1_ext
  %not2 = icmp eq i256 %or, 0
  %not2_ext = zext i1 %not2 to i256
  %loop_val = add i256 %not2_ext, 167
  store i256 %loop_val, ptr addrspace(5) null, align 1
  %t = icmp ult i256 %z, 256
  %t_ext = zext i1 %t to i256
  %lhs = add i256 %n, %t_ext
  %rhs = add i256 %n, 1
  %brk = icmp eq i256 %lhs, %rhs
  br i1 %brk, label %exit, label %latch

latch:
  %vn1 = and i256 %n, 1
  %p_next = icmp eq i256 %cmp_ext, %vn1
  %p_next_ext = zext i1 %p_next to i256
  br label %loop

exit:
  %res = xor i256 %or, 1
  ret i256 %res
}
