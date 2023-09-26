; RUN: llc < %s | FileCheck %s

target triple = "eravm"

; CHECK-LABEL: @test_add_code
define i256 @test_add_code(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK-NOT: add.ne r{{[0-9]+}}, r0, r{{[0-9]+}}
; CHECK: add.ne @CPI0_0[0], r[[REG:[0-9]+]], r[[REG]]

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = add i256 %bb, 10245387
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_add_imm
define i256 @test_add_imm(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK-NOT: add.ne r{{[0-9]+}}, r0, r{{[0-9]+}}
; CHECK: add.ne 1024, r[[REG:[0-9]+]], r[[REG]]

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = add i256 %bb, 1024
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_add_reg
define i256 @test_add_reg(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK-NOT: add.ne r{{[0-9]+}}, r0, r{{[0-9]+}}
; CHECK: add.ne r[[REG:[0-9]+]], r[[REG]], r[[REG]]

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = add i256 %bb, %bb
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_reg
define i256 @test_sub_reg(i256 %0, i256 %1, i256 %a) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; TODO CPR-1404: better scheduling 
; CHECK: add.ne r{{[0-9]+}}, r0, r{{[0-9]+}}
; TODO: sub.s.ne 1024, r[[REG:[0-9]+]], r[[REG]]

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = sub i256 %a, %bb
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_imm
define i256 @test_sub_imm(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK-NOT: add.ne r{{[0-9]+}}, r0, r{{[0-9]+}}
; CHECK: sub.s.ne 1024, r[[REG:[0-9]+]], r[[REG]]

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = sub i256 %bb, 1024
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_imm2
define i256 @test_sub_imm2(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK-NOT: add.ne r{{[0-9]+}}, r0, r{{[0-9]+}}
; CHECK: sub.ne 1024, r[[REG:[0-9]+]], r[[REG]]

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = sub i256 1024, %bb
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_code
define i256 @test_sub_code(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK: add.ne @CPI6_0[0], r[[REG:[0-9]+]], r[[REG]]

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = sub i256 %bb, 10242048
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_code2
define i256 @test_sub_code2(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; TODO: CPR-1404
; TODO: sub.ne

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = sub i256 10242048, %bb
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_mul_code
define i256 @test_sub_mul_code(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK: mul.ne @CPI8_0[0], r{{[0-9]+}}, r{{[0-9]+}}

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = mul i256 10242048, %bb
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_mul_imm
define i256 @test_sub_mul_imm(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK: mul.ne 1025, r[[REG:[0-9]+]], r[[REG]]

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = mul i256 1025, %bb
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_mul_reg
define i256 @test_sub_mul_reg(i256 %0, i256 %1, i256 %a) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; TODO: CPR-1404
; TODO: mul.ne

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = mul i256 %a, %bb
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_div_imm
define i256 @test_sub_div_imm(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK: div.ne 1025, r[[REG:[0-9]+]], r[[REG]]

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = udiv i256 1025, %bb
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_div_imm2
define i256 @test_sub_div_imm2(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK: div.s.ne 1025, r[[REG:[0-9]+]], r[[REG]]

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = udiv i256 %bb, 1025
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_div_code
define i256 @test_sub_div_code(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK: div.ne @CPI13_0[0], r{{[0-9]+}}, r{{[0-9]+}}

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = udiv i256 10242048, %bb
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_div_code2
define i256 @test_sub_div_code2(i256 %0, i256 %1) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; CHECK: div.s.ne @CPI14_0[0], r{{[0-9]+}}, r{{[0-9]+}}

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = udiv i256 %bb, 10242048
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}

; CHECK-LABEL: @test_sub_div_reg
define i256 @test_sub_div_reg(i256 %0, i256 %1, i256 %a) {
entry:
  %b = add i256 0, 1024
  %comparison_result = icmp eq i256 %0, 0
  br i1 %comparison_result, label %for_body, label %exit_label

; TODO: CPR-1404
; TODO: div.ne

for_body:
  %bb = phi i256 [ %b_merge_shifted, %for_body ], [ %b, %entry ]
  %2 = and i256 %bb, 1
  %trunc_result = icmp eq i256 %2, 0
  %addition_result = udiv i256 %bb, %a
  %b_merge = select i1 %trunc_result, i256 %bb, i256 %addition_result
  %b_merge_shifted = lshr i256 %b_merge, 1
  %3 = and i256 %1, 2
  %cmp = icmp eq i256 %3, 0
  br i1 %cmp, label %for_body, label %exit_label

exit_label:
  %result = phi i256 [%b, %entry], [%b_merge_shifted, %for_body]
  ret i256 %result
}