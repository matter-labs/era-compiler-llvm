; RUN: llc < %s

; calldata_struct_array_reencode test reduced with bugpoint.
; before the fix the test caused stack too deep failure because of unreachable
; slots mishandling in EVMStackSolver.
source_filename = "era-solidity/test/libsolidity/semanticTests/abiEncoderV2/calldata_struct_array_reencode.sol:C.runtime"
target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

; Function Attrs: nounwind willreturn memory(none)
declare i256 @llvm.evm.calldatasize() #0

; Function Attrs: nounwind willreturn memory(none)
declare i256 @llvm.evm.callvalue() #0

; Function Attrs: noreturn nounwind
declare void @llvm.evm.return(ptr addrspace(1), i256) #1

; Function Attrs: noreturn nounwind
declare void @llvm.evm.revert(ptr addrspace(1), i256) #1

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memmove.p1.p1.i256(ptr addrspace(1) nocapture writeonly, ptr addrspace(1) nocapture readonly, i256, i1 immarg) #2

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p1.p2.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(2) noalias nocapture readonly, i256, i1 immarg) #2

; Function Attrs: null_pointer_is_valid
define dso_local fastcc void @main() unnamed_addr #3 {
entry:
  br i1 poison, label %"block_rt_7/0", label %"block_rt_2/0"

"block_rt_2/0":                                   ; preds = %conditional_rt_181_join_block, %"block_rt_165/1", %entry
  unreachable

"block_rt_7/0":                                   ; preds = %entry
  %calldata_load_result2781 = load i256, ptr addrspace(2) inttoptr (i256 4 to ptr addrspace(2)), align 4
  %calldata_load_result2605 = load i256, ptr addrspace(2) poison, align 1
  %addition_result2659 = add nuw nsw i256 %calldata_load_result2781, 36
  %subtraction_result1906 = add i256 0, -31
  br label %conditional_rt_187_join_block

"block_rt_165/1":                                 ; preds = %conditional_rt_181_join_block
  %addition_result3756 = add i256 0, %addition_result4054
  %addition_result3239 = add i256 0, %addition_result3756
  %addition_result3251 = add i256 %addition_result3239, 32
  %comparison_result3399.not = icmp sgt i256 %addition_result3251, poison
  br i1 %comparison_result3399.not, label %"block_rt_2/0", label %shift_left_join3672

"block_rt_181/0":                                 ; preds = %shift_left_join3672
  %addition_result4058 = add i256 %stack_var_012.36954, 32
  %addition_result4064 = add i256 %stack_var_011.36953, 32
  %addition_result4044 = add nuw i256 %stack_var_013.36955, 1
  %comparison_result4003.not = icmp ult i256 %addition_result4044, %calldata_load_result2605
  br i1 %comparison_result4003.not, label %conditional_rt_187_join_block, label %"block_rt_187/0.loopexit"

"block_rt_187/0.loopexit":                        ; preds = %"block_rt_181/0"
  unreachable

shift_left_join3672:                              ; preds = %"block_rt_165/1"
  %addition_result3684 = add i256 %stack_var_021.0.in6947, 128
  %calldatacopy_destination_pointer3688 = inttoptr i256 %addition_result3684 to ptr addrspace(1)
  tail call void @llvm.memcpy.p1.p2.i256(ptr addrspace(1) align 1 %calldatacopy_destination_pointer3688, ptr addrspace(2) align 1 poison, i256 poison, i1 false)
  %addition_result3694 = add i256 poison, %stack_var_021.06950
  %addition_result3972 = add i256 %stack_var_022.06948, 32
  %addition_result3978 = add i256 %stack_var_017.26946, 32
  %stack_var_021.0 = add i256 %addition_result3694, 64
  br i1 %comparison_result3909.not, label %conditional_rt_181_join_block, label %"block_rt_181/0"

conditional_rt_181_join_block:                    ; preds = %conditional_rt_187_join_block, %shift_left_join3672
  %stack_var_021.06950 = phi i256 [ poison, %conditional_rt_187_join_block ], [ %stack_var_021.0, %shift_left_join3672 ]
  %comparison_result3909.not = phi i1 [ true, %conditional_rt_187_join_block ], [ false, %shift_left_join3672 ]
  %stack_var_022.06948 = phi i256 [ %addition_result4054, %conditional_rt_187_join_block ], [ %addition_result3972, %shift_left_join3672 ]
  %stack_var_021.0.in6947 = phi i256 [ %stack_var_010.26952, %conditional_rt_187_join_block ], [ %addition_result3694, %shift_left_join3672 ]
  %stack_var_017.26946 = phi i256 [ %stack_var_010.26952, %conditional_rt_187_join_block ], [ %addition_result3978, %shift_left_join3672 ]
  %subtraction_result3918 = sub i256 %stack_var_021.06950, %stack_var_010.26952
  %memory_store_pointer3922 = inttoptr i256 %stack_var_017.26946 to ptr addrspace(1)
  store i256 %subtraction_result3918, ptr addrspace(1) %memory_store_pointer3922, align 1
  %calldataload_pointer1898 = inttoptr i256 %stack_var_022.06948 to ptr addrspace(2)
  %comparison_result1913.not = icmp slt i256 0, %addition_result1909
  br i1 %comparison_result1913.not, label %"block_rt_165/1", label %"block_rt_2/0"

conditional_rt_187_join_block:                    ; preds = %"block_rt_181/0", %"block_rt_7/0"
  %stack_var_013.36955 = phi i256 [ 0, %"block_rt_7/0" ], [ %addition_result4044, %"block_rt_181/0" ]
  %stack_var_012.36954 = phi i256 [ %addition_result2659, %"block_rt_7/0" ], [ %addition_result4058, %"block_rt_181/0" ]
  %stack_var_011.36953 = phi i256 [ 224, %"block_rt_7/0" ], [ %addition_result4064, %"block_rt_181/0" ]
  %stack_var_010.26952 = phi i256 [ poison, %"block_rt_7/0" ], [ %stack_var_021.0, %"block_rt_181/0" ]
  %memory_store_pointer4021 = inttoptr i256 %stack_var_011.36953 to ptr addrspace(1)
  %calldataload_pointer4024 = inttoptr i256 %stack_var_012.36954 to ptr addrspace(2)
  %calldata_load_result4025 = load i256, ptr addrspace(2) %calldataload_pointer4024, align 1
  %addition_result4054 = add i256 %calldata_load_result4025, %addition_result2659
  %addition_result1909 = sub i256 %subtraction_result1906, %addition_result4054
  br label %conditional_rt_181_join_block
}

; Function Attrs: null_pointer_is_valid
define dso_local fastcc void @main2() unnamed_addr #3 {
entry:
  switch i32 poison, label %"block_rt_2/0" [
    i32 1931402874, label %"block_rt_7/0"
  ]

"block_rt_2/0":                                   ; preds = %conditional_rt_181_join_block, %"block_rt_165/1", %entry
  unreachable

"block_rt_7/0":                                   ; preds = %entry
  %calldata_load_result2781 = load i256, ptr addrspace(2) inttoptr (i256 4 to ptr addrspace(2)), align 4
  %addition_result2798 = add nuw nsw i256 %calldata_load_result2781, 4
  %calldataload_pointer2604 = inttoptr i256 %addition_result2798 to ptr addrspace(2)
  %calldata_load_result2605 = load i256, ptr addrspace(2) %calldataload_pointer2604, align 1
  %addition_result2659 = add nuw nsw i256 %calldata_load_result2781, 36
  %subtraction_result1906 = add i256 0, -31
  br label %conditional_rt_187_join_block

"block_rt_165/1":                                 ; preds = %conditional_rt_181_join_block
  %addition_result3756 = add i256 %calldata_load_result1899, %addition_result4054
  %addition_result3239 = add i256 0, %addition_result3756
  %calldataload_pointer3244 = inttoptr i256 %addition_result3239 to ptr addrspace(2)
  %calldata_load_result3245 = load i256, ptr addrspace(2) %calldataload_pointer3244, align 1
  %addition_result3251 = add i256 %addition_result3239, 32
  %shift_left_non_overflow_result3390 = shl nuw nsw i256 %calldata_load_result3245, 5
  %comparison_result3399.not = icmp sgt i256 %addition_result3251, poison
  br i1 %comparison_result3399.not, label %"block_rt_2/0", label %shift_left_join3672

"block_rt_181/0":                                 ; preds = %shift_left_join3672
  %addition_result4064 = add i256 %stack_var_011.36953, 32
  %addition_result4044 = add nuw i256 %stack_var_013.36955, 1
  %comparison_result4003.not = icmp ult i256 %addition_result4044, %calldata_load_result2605
  br i1 %comparison_result4003.not, label %conditional_rt_187_join_block, label %"block_rt_187/0.loopexit"

"block_rt_187/0.loopexit":                        ; preds = %"block_rt_181/0"
  %addition_result439 = add i256 %stack_var_021.0, 32
  %memory_store_pointer442 = inttoptr i256 %addition_result439 to ptr addrspace(1)
  store i256 0, ptr addrspace(1) %memory_store_pointer442, align 1
  unreachable

shift_left_join3672:                              ; preds = %"block_rt_165/1"
  %addition_result3535 = add i256 %stack_var_021.0.in6947, 96
  %memory_store_pointer3538 = inttoptr i256 %addition_result3535 to ptr addrspace(1)
  store i256 %calldata_load_result3245, ptr addrspace(1) %memory_store_pointer3538, align 1
  %addition_result3694 = add i256 %shift_left_non_overflow_result3390, %stack_var_021.06950
  %addition_result3972 = add i256 %stack_var_022.06948, 32
  %addition_result3978 = add i256 %stack_var_017.26946, 32
  %stack_var_021.0 = add i256 %addition_result3694, 64
  br i1 %comparison_result3909.not, label %conditional_rt_181_join_block, label %"block_rt_181/0"

conditional_rt_181_join_block:                    ; preds = %conditional_rt_187_join_block, %shift_left_join3672
  %stack_var_021.06950 = phi i256 [ poison, %conditional_rt_187_join_block ], [ %stack_var_021.0, %shift_left_join3672 ]
  %comparison_result3909.not = phi i1 [ true, %conditional_rt_187_join_block ], [ false, %shift_left_join3672 ]
  %stack_var_022.06948 = phi i256 [ %addition_result4054, %conditional_rt_187_join_block ], [ %addition_result3972, %shift_left_join3672 ]
  %stack_var_021.0.in6947 = phi i256 [ %stack_var_010.26952, %conditional_rt_187_join_block ], [ %addition_result3694, %shift_left_join3672 ]
  %stack_var_017.26946 = phi i256 [ %stack_var_010.26952, %conditional_rt_187_join_block ], [ %addition_result3978, %shift_left_join3672 ]
  %subtraction_result3918 = sub i256 %stack_var_021.06950, %stack_var_010.26952
  %memory_store_pointer3922 = inttoptr i256 %stack_var_017.26946 to ptr addrspace(1)
  store i256 %subtraction_result3918, ptr addrspace(1) %memory_store_pointer3922, align 1
  %calldataload_pointer1898 = inttoptr i256 %stack_var_022.06948 to ptr addrspace(2)
  %calldata_load_result1899 = load i256, ptr addrspace(2) %calldataload_pointer1898, align 1
  %comparison_result1913.not = icmp slt i256 %calldata_load_result1899, %addition_result1909
  br i1 %comparison_result1913.not, label %"block_rt_165/1", label %"block_rt_2/0"

conditional_rt_187_join_block:                    ; preds = %"block_rt_181/0", %"block_rt_7/0"
  %stack_var_013.36955 = phi i256 [ 0, %"block_rt_7/0" ], [ %addition_result4044, %"block_rt_181/0" ]
  %stack_var_011.36953 = phi i256 [ 224, %"block_rt_7/0" ], [ %addition_result4064, %"block_rt_181/0" ]
  %stack_var_010.26952 = phi i256 [ poison, %"block_rt_7/0" ], [ %stack_var_021.0, %"block_rt_181/0" ]
  %memory_store_pointer4021 = inttoptr i256 %stack_var_011.36953 to ptr addrspace(1)
  %calldata_load_result4025 = load i256, ptr addrspace(2) poison, align 1
  %addition_result4054 = add i256 %calldata_load_result4025, %addition_result2659
  %addition_result1909 = sub i256 %subtraction_result1906, %addition_result4054
  br label %conditional_rt_181_join_block
}

; Function Attrs: null_pointer_is_valid
declare void @__entry() local_unnamed_addr #3

attributes #0 = { nounwind willreturn memory(none) }
attributes #1 = { noreturn nounwind }
attributes #2 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { null_pointer_is_valid }
