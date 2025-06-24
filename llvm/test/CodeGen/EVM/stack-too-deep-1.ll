; REQUIRES: asserts
; RUN: llc -evm-stack-region-offset=128 -evm-stack-region-size=32 --debug-only=evm-stack-solver < %s 2>&1 | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

; Check that the stack solver detects unreachable slots, generates spills for them, and
; succesfully compiles the function. Also, check that we allocated the exact amount of
; stack space needed for the function, without any warnings about allocated stack region size.

; CHECK: Unreachable slots found: 2, iteration: 1
; CHECK: Spilling 1 registers
; CHECK-NOT: warning: allocated stack region size:

define dso_local fastcc void @main() unnamed_addr {
entry:
  br label %"block_rt_19/0"

"block_rt_19/0":                                  ; preds = %entry
  %addition_result2158 = add nuw nsw i256 1, 20
  %and_result2674 = and i256 %addition_result2158, 255
  %trunc = trunc i256 %addition_result2158 to i8
  %comparison_result3799.not = icmp eq i256 %and_result2674, 3
  %comparison_result6184.not = icmp eq i256 %and_result2674, 24
  %comparison_result7522.not = icmp eq i256 %and_result2674, 26
  %comparison_result9235.not = icmp eq i256 %and_result2674, 28
  br label %"block_rt_44/7"

"block_rt_43/7":                                  ; preds = %remainder_join12933, %"block_rt_54/7.thread", %"block_rt_44/7"
  unreachable

"block_rt_44/7":                                  ; preds = %"block_rt_19/0"
  switch i8 %trunc, label %"block_rt_51/7" [
    i8 1, label %"block_rt_43/7"
    i8 21, label %conditional_rt_49_join_block2921
    i8 11, label %"block_rt_46/7"
  ]

"block_rt_46/7":                                  ; preds = %"block_rt_51/7", %"block_rt_44/7"
  unreachable

"block_rt_51/7":                                  ; preds = %"block_rt_57/7", %"block_rt_44/7"
  %stack_var_010.1 = phi i256 [ %multiplication_result1217, %"block_rt_57/7" ], [ 1, %"block_rt_44/7" ]
  switch i8 %trunc, label %"block_rt_54/7.thread" [
    i8 22, label %"block_rt_46/7"
    i8 33, label %"block_rt_54/7"
  ]

"block_rt_54/7":                                  ; preds = %"block_rt_51/7"
  %comparison_result3562 = icmp ugt i256 %stack_var_010.1, 1
  unreachable

"block_rt_56/7.outer":                            ; preds = %"block_rt_56/7.preheader", %"block_rt_70/7"
  %stack_var_011.8.ph = phi i256 [ 0, %"block_rt_56/7.preheader" ], [ %addition_result2182, %"block_rt_70/7" ]
  br label %"block_rt_56/7"

"block_rt_56/7":                                  ; preds = %"block_rt_64/7", %"block_rt_56/7.outer"
  %stack_var_011.8 = phi i256 [ %addition_result2182, %"block_rt_64/7" ], [ %stack_var_011.8.ph, %"block_rt_56/7.outer" ]
  %and_result2179 = and i256 %stack_var_011.8, 255
  %addition_result2182 = add nuw nsw i256 %and_result2179, 2
  br label %"block_rt_64/7"

"block_rt_57/7":                                  ; preds = %"block_rt_64/7"
  %multiplication_result1217 = shl nuw nsw i256 %stack_var_010.1, 1
  br label %"block_rt_51/7"

"block_rt_64/7":                                  ; preds = %"block_rt_56/7"
  switch i8 %trunc, label %conditional_rt_68_join_block5874 [
    i8 12, label %"block_rt_56/7"
    i8 23, label %"block_rt_57/7"
  ]

"block_rt_70/7":                                  ; preds = %conditional_rt_68_join_block5874
  %comparison_result6061 = icmp ugt i256 %and_result5861, 2
  %or.cond = or i1 %comparison_result6184.not, %comparison_result6061
  br i1 %or.cond, label %"block_rt_56/7.outer", label %"block_rt_73/7"

"block_rt_73/7":                                  ; preds = %"block_rt_80/7", %"block_rt_70/7"
  %stack_var_013.2 = phi i256 [ %addition_result6585, %"block_rt_80/7" ], [ 10, %"block_rt_70/7" ]
  %and_result6465 = and i256 %stack_var_013.2, 255
  br i1 false, label %"block_rt_123/8", label %conditional_rt_74_join_block6475

"block_rt_80/7":                                  ; preds = %"block_rt_86/7"
  %addition_result6585 = add nuw nsw i256 %stack_var_013.2, 1
  br label %"block_rt_73/7"

"block_rt_86/7":                                  ; preds = %remainder_join12901
  br i1 %comparison_result7522.not, label %"block_rt_80/7", label %"block_rt_88/7.outer"

"block_rt_88/7.outer":                            ; preds = %"block_rt_101/7", %"block_rt_86/7"
  %stack_var_008.19.ph = phi i256 [ poison, %"block_rt_86/7" ], [ %stack_var_008.20, %"block_rt_101/7" ]
  %stack_var_015.1.ph = phi i256 [ 10, %"block_rt_86/7" ], [ %addition_result2206, %"block_rt_101/7" ]
  %and_result7774 = and i256 %stack_var_015.1.ph, 255
  switch i8 %trunc, label %"block_rt_92/7" [
    i8 7, label %conditional_rt_90_join_block7798
    i8 27, label %"block_rt_131/7.backedge"
  ]

"block_rt_89/7":                                  ; preds = %"block_rt_95/7"
  unreachable

"block_rt_131/7.outer":                           ; preds = %conditional_rt_74_join_block6475
  br label %"block_rt_131/7"

"block_rt_92/7":                                  ; preds = %"block_rt_88/7.outer"
  %addition_result2206 = add nuw nsw i256 %and_result7774, 1
  br label %"block_rt_95/7"

"block_rt_95/7":                                  ; preds = %"block_rt_92/7"
  %comparison_result8476.not = icmp eq i256 %addition_result2206, 16
  br i1 %comparison_result8476.not, label %"block_rt_89/7", label %"block_rt_96/7"

"block_rt_96/7":                                  ; preds = %"block_rt_95/7"
  switch i8 %trunc, label %"block_rt_101/7.preheader" [
    i8 31, label %"block_rt_97/7"
    i8 32, label %"block_rt_99/7"
  ]

"block_rt_97/7":                                  ; preds = %"block_rt_96/7"
  unreachable

"block_rt_99/7":                                  ; preds = %"block_rt_96/7"
  unreachable

"block_rt_101/7.preheader":                       ; preds = %"block_rt_96/7"
  br label %"block_rt_101/7"

"block_rt_101/7":                                 ; preds = %"block_rt_103/7", %"block_rt_101/7.preheader"
  %stack_var_008.20 = phi i256 [ %stack_var_008.21, %"block_rt_103/7" ], [ %stack_var_008.19.ph, %"block_rt_101/7.preheader" ]
  %or.cond20853 = or i1 %comparison_result9235.not, false
  br i1 %or.cond20853, label %"block_rt_88/7.outer", label %remainder_join12933

"block_rt_103/7":                                 ; preds = %remainder_join12933, %"block_rt_108/7"
  %stack_var_008.21 = phi i256 [ %addition_result10002, %"block_rt_108/7" ], [ %stack_var_008.20, %remainder_join12933 ]
  br label %"block_rt_101/7"

"block_rt_108/7":                                 ; preds = %remainder_join12933
  %and_result9999 = and i256 %stack_var_008.20, 18446744073709551615
  %addition_result10002 = add nuw nsw i256 %and_result9999, 1
  br label %"block_rt_103/7"

"block_rt_123/8":                                 ; preds = %conditional_rt_74_join_block6475, %"block_rt_73/7"
  %addition_result12201 = add nuw nsw i256 %and_result5861, 1
  br label %conditional_rt_68_join_block5874

"block_rt_131/7":                                 ; preds = %"block_rt_131/7.backedge", %"block_rt_131/7.outer"
  %stack_var_014.1 = phi i256 [ 7, %"block_rt_131/7.outer" ], [ %subtraction_result7209, %"block_rt_131/7.backedge" ]
  %and_result7206 = and i256 %stack_var_014.1, 255
  %subtraction_result7209 = add nsw i256 %and_result7206, -1
  br label %remainder_join12901

conditional_rt_49_join_block2921:                 ; preds = %"block_rt_44/7"
  unreachable

"block_rt_54/7.thread":                           ; preds = %"block_rt_51/7"
  br i1 %comparison_result3799.not, label %"block_rt_43/7", label %"block_rt_56/7.preheader"

"block_rt_56/7.preheader":                        ; preds = %"block_rt_54/7.thread"
  br label %"block_rt_56/7.outer"

conditional_rt_68_join_block5874:                 ; preds = %"block_rt_123/8", %"block_rt_64/7"
  %stack_var_012.2 = phi i256 [ %addition_result12201, %"block_rt_123/8" ], [ 1, %"block_rt_64/7" ]
  %and_result5861 = and i256 %stack_var_012.2, 255
  br label %"block_rt_70/7"

conditional_rt_74_join_block6475:                 ; preds = %"block_rt_73/7"
  switch i8 %trunc, label %"block_rt_131/7.outer" [
    i8 5, label %conditional_rt_76_join_block6491
    i8 25, label %"block_rt_123/8"
  ]

conditional_rt_76_join_block6491:                 ; preds = %conditional_rt_74_join_block6475
  unreachable

conditional_rt_90_join_block7798:                 ; preds = %"block_rt_88/7.outer"
  unreachable

remainder_join12901:                              ; preds = %"block_rt_131/7"
  %remainder_result_non_zero12904 = and i256 %subtraction_result7209, 1
  br i1 poison, label %"block_rt_131/7.backedge", label %"block_rt_86/7"

"block_rt_131/7.backedge":                        ; preds = %remainder_join12901, %"block_rt_88/7.outer"
  br label %"block_rt_131/7"

remainder_join12933:                              ; preds = %"block_rt_101/7"
  switch i8 %trunc, label %"block_rt_108/7" [
    i8 8, label %"block_rt_43/7"
    i8 13, label %"block_rt_103/7"
  ]
}
