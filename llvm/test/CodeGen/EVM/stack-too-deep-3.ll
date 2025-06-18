; REQUIRES: asserts
; RUN: llc -evm-stack-region-offset=128 -evm-stack-region-size=32 --debug-only=evm-stack-solver < %s 2>&1 | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

declare void @llvm.memmove.p1.p1.i256(ptr addrspace(1) nocapture writeonly, ptr addrspace(1) nocapture readonly, i256, i1 immarg) #0

; Check that the stack solver detects unreachable slots, generates spills for them, and
; succesfully compiles the function. Also, check that we allocated the exact amount of
; stack space needed for the function, without any warnings about allocated stack region size.

; CHECK: Unreachable slots found: 1, iteration: 1
; CHECK: Spilling 1 registers
; CHECK-NOT: warning: allocated stack region size:

define dso_local fastcc void @main() unnamed_addr {
entry:
  br i1 poison, label %"block_rt_7/0", label %"block_rt_2/0"

"block_rt_2/0":                                   ; preds = %conditional_rt_187_join_block, %conditional_rt_181_join_block, %"block_rt_158/3", %entry
  unreachable

"block_rt_7/0":                                   ; preds = %entry
  %calldata_load_result2781 = load i256, ptr addrspace(2) inttoptr (i256 4 to ptr addrspace(2)), align 4
  %addition_result2798 = add nuw nsw i256 %calldata_load_result2781, 4
  %calldataload_pointer2604 = inttoptr i256 %addition_result2798 to ptr addrspace(2)
  %calldata_load_result2605 = load i256, ptr addrspace(2) %calldataload_pointer2604, align 1
  %addition_result2659 = add nuw nsw i256 %calldata_load_result2781, 36
  %addition_result1186 = add nsw i256 0, -99
  %subtraction_result1906 = add nsw i256 0, -31
  br label %conditional_rt_187_join_block

"block_rt_158/3":                                 ; preds = %conditional_rt_181_join_block
  %addition_result3239 = add i256 0, %addition_result3756
  %calldataload_pointer3244 = inttoptr i256 %addition_result3239 to ptr addrspace(2)
  %calldata_load_result3245 = load i256, ptr addrspace(2) %calldataload_pointer3244, align 1
  %addition_result3251 = add i256 %addition_result3239, 32
  %shift_left_non_overflow_result3390 = shl nuw nsw i256 %calldata_load_result3245, 5
  %subtraction_result3395 = sub i256 0, %shift_left_non_overflow_result3390
  %comparison_result3399.not = icmp sgt i256 %addition_result3251, %subtraction_result3395
  br i1 %comparison_result3399.not, label %"block_rt_2/0", label %"block_rt_160/3"

"block_rt_160/3":                                 ; preds = %"block_rt_158/3"
  %memory_store_pointer3530 = inttoptr i256 %stack_var_021.06950 to ptr addrspace(1)
  %addition_result3535 = add i256 %stack_var_021.0.in6947, 96
  %memory_store_pointer3538 = inttoptr i256 %addition_result3535 to ptr addrspace(1)
  store i256 %calldata_load_result3245, ptr addrspace(1) %memory_store_pointer3538, align 1
  %addition_result3694 = add i256 %shift_left_non_overflow_result3390, %stack_var_021.06950
  %addition_result3972 = add i256 %stack_var_022.06948, 32
  %addition_result3978 = add i256 %stack_var_017.26946, 32
  %stack_var_021.0 = add i256 %addition_result3694, 64
  br i1 %comparison_result3909.not, label %conditional_rt_181_join_block, label %"block_rt_181/0"

"block_rt_181/0":                                 ; preds = %"block_rt_160/3"
  %addition_result4058 = add i256 %stack_var_012.36954, 32
  %addition_result4064 = add i256 %stack_var_011.36953, 32
  %addition_result4044 = add nuw i256 %stack_var_013.36955, 1
  %comparison_result4003.not = icmp ult i256 %addition_result4044, %calldata_load_result2605
  br i1 %comparison_result4003.not, label %conditional_rt_187_join_block, label %"block_rt_187/0.loopexit"

"block_rt_187/0.loopexit":                        ; preds = %"block_rt_181/0"
  %addition_result451 = add i256 %stack_var_021.0, 64
  %mcopy_destination455 = inttoptr i256 %addition_result451 to ptr addrspace(1)
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) align 1 %mcopy_destination455, ptr addrspace(1) align 1 poison, i256 poison, i1 false)
  unreachable

"block_rt_188/0":                                 ; preds = %conditional_rt_187_join_block
  %addition_result4054 = add i256 0, %addition_result2659
  br label %conditional_rt_181_join_block

conditional_rt_181_join_block:                    ; preds = %"block_rt_188/0", %"block_rt_160/3"
  %stack_var_021.06950 = phi i256 [ poison, %"block_rt_188/0" ], [ %stack_var_021.0, %"block_rt_160/3" ]
  %comparison_result3909.not = phi i1 [ true, %"block_rt_188/0" ], [ false, %"block_rt_160/3" ]
  %stack_var_022.06948 = phi i256 [ %addition_result4054, %"block_rt_188/0" ], [ %addition_result3972, %"block_rt_160/3" ]
  %stack_var_021.0.in6947 = phi i256 [ %stack_var_010.26952, %"block_rt_188/0" ], [ %addition_result3694, %"block_rt_160/3" ]
  %stack_var_017.26946 = phi i256 [ %stack_var_010.26952, %"block_rt_188/0" ], [ %addition_result3978, %"block_rt_160/3" ]
  %subtraction_result3918 = sub i256 %stack_var_021.06950, %stack_var_010.26952
  %memory_store_pointer3922 = inttoptr i256 %stack_var_017.26946 to ptr addrspace(1)
  store i256 %subtraction_result3918, ptr addrspace(1) %memory_store_pointer3922, align 1
  %calldataload_pointer1898 = inttoptr i256 %stack_var_022.06948 to ptr addrspace(2)
  %addition_result3756 = add i256 0, %addition_result4054
  %addition_result1811 = sub i256 %subtraction_result1906, %addition_result3756
  %comparison_result1815.not = icmp slt i256 0, %addition_result1811
  br i1 %comparison_result1815.not, label %"block_rt_158/3", label %"block_rt_2/0"

conditional_rt_187_join_block:                    ; preds = %"block_rt_181/0", %"block_rt_7/0"
  %stack_var_013.36955 = phi i256 [ 0, %"block_rt_7/0" ], [ %addition_result4044, %"block_rt_181/0" ]
  %stack_var_012.36954 = phi i256 [ %addition_result2659, %"block_rt_7/0" ], [ %addition_result4058, %"block_rt_181/0" ]
  %stack_var_011.36953 = phi i256 [ 224, %"block_rt_7/0" ], [ %addition_result4064, %"block_rt_181/0" ]
  %stack_var_010.26952 = phi i256 [ poison, %"block_rt_7/0" ], [ %stack_var_021.0, %"block_rt_181/0" ]
  %memory_store_pointer4021 = inttoptr i256 %stack_var_011.36953 to ptr addrspace(1)
  %calldataload_pointer4024 = inttoptr i256 %stack_var_012.36954 to ptr addrspace(2)
  %comparison_result4030.not = icmp slt i256 0, %addition_result1186
  br i1 %comparison_result4030.not, label %"block_rt_188/0", label %"block_rt_2/0"
}

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
