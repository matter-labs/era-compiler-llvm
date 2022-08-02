; RUN: llc -stop-before verify < %s | FileCheck %s
target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm-unknown-unknown"

declare i32 @__personality()

declare { i256, i1 }* @__farcall(i256, i256, { i256, i1 }*) local_unnamed_addr

declare { i256, i1 }* @__staticcall(i256, i256, { i256, i1 }*) local_unnamed_addr
; Function Attrs: noreturn
define fastcc void @__runtime() unnamed_addr #0 personality i32 ()* @__personality {
entry:
  %check_extcodesize_this_address = tail call i256 @llvm.syncvm.this()
  %check_address_is_account_code_storage = icmp eq i256 %check_extcodesize_this_address, 32770
  br i1 %check_address_is_account_code_storage, label %check_extcodesize_join, label %check_extcodesize_call

check_extcodesize_call:                           ; preds = %entry
  store i256 -33873442639124413528343236154223523495557942579052471210631277873373353839196, i256 addrspace(1)* inttoptr (i256 16775168 to i256 addrspace(1)*), align 2048
  store i256 %check_extcodesize_this_address, i256 addrspace(1)* inttoptr (i256 16775172 to i256 addrspace(1)*), align 4
  %call_result_pointer = alloca { i256, i1 }, align 32
  %call = call { i256, i1 }* @__staticcall(i256 32770, i256 664082786653560633344, { i256, i1 }* nonnull %call_result_pointer)
  %call_result_abi_data_pointer = getelementptr { i256, i1 }, { i256, i1 }* %call, i256 0, i32 0
  %call_result_abi_data = load i256, i256* %call_result_abi_data_pointer, align 32
  %call_child_data_offset = and i256 %call_result_abi_data, 18446744073709551615
  %call_child_data_length_shifted = lshr i256 %call_result_abi_data, 64
  %call_child_data_length = and i256 %call_child_data_length_shifted, 18446744073709551615
  store i256 %call_child_data_offset, i256 addrspace(1)* inttoptr (i256 16777120 to i256 addrspace(1)*), align 32
  store i256 %call_child_data_length, i256 addrspace(1)* inttoptr (i256 16777088 to i256 addrspace(1)*), align 128
  %call_result_status_code_pointer = getelementptr { i256, i1 }, { i256, i1 }* %call, i256 0, i32 1
  %call_result_status_code_boolean = load i1, i1* %call_result_status_code_pointer, align 32
  br i1 %call_result_status_code_boolean, label %call_join_block, label %check_extcodesize_revert

check_extcodesize_revert:                         ; preds = %check_extcodesize_call, %call_join_block
  call void @llvm.syncvm.return(i256 0)
  unreachable

check_extcodesize_join:                           ; preds = %call_join_block, %entry
  store i256 128, i256 addrspace(1)* inttoptr (i256 64 to i256 addrspace(1)*), align 64
  %get_u128_value = call i256 @llvm.syncvm.getu128()
  %if_condition_compared.not = icmp eq i256 %get_u128_value, 0
  br i1 %if_condition_compared.not, label %if_join, label %if_main

call_join_block:                                  ; preds = %check_extcodesize_call
  %call_child_pointer = inttoptr i256 %call_child_data_offset to i256 addrspace(3)*
  %call_child_address = load i256, i256 addrspace(3)* %call_child_pointer, align 1
  %check_extcodesize_non_zero.not = icmp eq i256 %call_child_address, 0
  br i1 %check_extcodesize_non_zero.not, label %check_extcodesize_revert, label %check_extcodesize_join

if_main:                                          ; preds = %check_extcodesize_join
  call void @llvm.syncvm.revert(i256 0)
  unreachable

if_join:                                          ; preds = %check_extcodesize_join
  %calldata_size = load i256, i256 addrspace(1)* inttoptr (i256 16777152 to i256 addrspace(1)*), align 64
  %comparison_result = icmp slt i256 %calldata_size, 32
  br i1 %comparison_result, label %if_main2, label %if_join3

if_main2:                                         ; preds = %if_join
  call void @llvm.syncvm.revert(i256 0)
  unreachable

if_join3:                                         ; preds = %if_join
  %calldata_parent_offset = load i256, i256 addrspace(1)* inttoptr (i256 16777184 to i256 addrspace(1)*), align 32
  %calldata_pointer = inttoptr i256 %calldata_parent_offset to i256 addrspace(2)*
  %calldata_value = load i256, i256 addrspace(2)* %calldata_pointer, align 1
  %and_result = and i256 %calldata_value, 1461501637330902918203684832716283019655932542975
  %0 = icmp ugt i256 %calldata_value, 1461501637330902918203684832716283019655932542975
  br i1 %0, label %if_main13, label %calldata_copy_default_block

if_main13:                                        ; preds = %if_join3
  call void @llvm.syncvm.revert(i256 0)
  unreachable

calldata_copy_default_block:                      ; preds = %if_join3
  call void @llvm.memcpy.p1i256.p2i256.i256(i256 addrspace(1)* nonnull align 128 inttoptr (i256 128 to i256 addrspace(1)*), i256 addrspace(2)* align 1 %calldata_pointer, i256 %calldata_size, i1 false)
  %calldata_size20.pre = load i256, i256 addrspace(1)* inttoptr (i256 16777152 to i256 addrspace(1)*), align 64
  %phi.bo = add i256 %calldata_size20.pre, 128
  %phi.cast = inttoptr i256 %phi.bo to i256 addrspace(1)*
  store i256 0, i256 addrspace(1)* %phi.cast, align 1
  %ergs_left = call i256 @llvm.syncvm.ergsleft()
  %cond = icmp eq i256 %and_result, 4
  br i1 %cond, label %contract_call_identity_block, label %contract_call_value_zero_block

contract_call_identity_block:                     ; preds = %calldata_copy_default_block
  %return_data_value.pr = load i256, i256 addrspace(1)* inttoptr (i256 16777088 to i256 addrspace(1)*), align 128
  br label %contract_call_join_block

contract_call_join_block:                         ; preds = %contract_call_value_join_block, %contract_call_identity_block
  %_383 = phi i256 [ %storemerge98, %contract_call_value_join_block ], [ %return_data_value.pr, %contract_call_identity_block ]
  %switch_case_condition_0 = icmp eq i256 %_383, 0
  br i1 %switch_case_condition_0, label %switch_join, label %switch_case_constant_2

contract_call_value_zero_block:                   ; preds = %calldata_copy_default_block
  %calldata_size24 = load i256, i256 addrspace(1)* inttoptr (i256 16777152 to i256 addrspace(1)*), align 64
  %abi_data_input_length_truncated30 = and i256 %calldata_size24, 4294967295
  %abi_data_gas_truncated31 = shl i256 %ergs_left, 32
  %abi_data_gas_shifted32 = and i256 %abi_data_gas_truncated31, 18446744069414584320
  %abi_data_gas_add_input_length33 = or i256 %abi_data_input_length_truncated30, %abi_data_gas_shifted32
  %abi_data_gas_add_input_length_shifted34 = shl nuw nsw i256 %abi_data_gas_add_input_length33, 64
  %abi_data_result35 = or i256 %abi_data_gas_add_input_length_shifted34, 128
  %mimic_call_result_pointer38 = alloca { i256, i1 }, align 32
; CHECK: %1 = or i256 %abi_data_result{{[0-9]*}}, 1606938044258990275541962092341162602522202993782792835301376
; CHECK: invoke { i256, i1 }* @__farcall(i256 %and_result, i256 %1, { i256, i1 }* nonnull %mimic_call_result_pointer38)
  %contract_call_external40 = invoke { i256, i1 }* @__farcall(i256 %and_result, i256 %abi_data_result35, { i256, i1 }* nonnull %mimic_call_result_pointer38)
          to label %join_block37 unwind label %catch_block36

contract_call_value_join_block:                   ; preds = %catch_block36, %join_block37
  %storemerge99 = phi i256 [ %contract_call_child_data_offset46, %join_block37 ], [ 0, %catch_block36 ]
  %storemerge98 = phi i256 [ %contract_call_child_data_length48, %join_block37 ], [ 0, %catch_block36 ]
  store i256 %storemerge99, i256 addrspace(1)* inttoptr (i256 16777120 to i256 addrspace(1)*), align 32
  store i256 %storemerge98, i256 addrspace(1)* inttoptr (i256 16777088 to i256 addrspace(1)*), align 128
  br label %contract_call_join_block

catch_block36:                                    ; preds = %contract_call_value_zero_block
  %landing39 = landingpad { i8*, i32 }
          catch i8* null
  br label %contract_call_value_join_block

join_block37:                                     ; preds = %contract_call_value_zero_block
  %contract_call_external_result_abi_data_pointer41 = getelementptr { i256, i1 }, { i256, i1 }* %contract_call_external40, i256 0, i32 0
  %contract_call_external_result_abi_data42 = load i256, i256* %contract_call_external_result_abi_data_pointer41, align 32
  %contract_call_child_data_offset46 = and i256 %contract_call_external_result_abi_data42, 18446744073709551615
  %contract_call_child_data_length_shifted47 = lshr i256 %contract_call_external_result_abi_data42, 64
  %contract_call_child_data_length48 = and i256 %contract_call_child_data_length_shifted47, 18446744073709551615
  br label %contract_call_value_join_block

switch_join:                                      ; preds = %contract_call_join_block, %if_join80
  %data90 = phi i256 [ %phi.bo107, %if_join80 ], [ 1180591620717411303552, %contract_call_join_block ]
  call void @llvm.syncvm.return(i256 %data90)
  unreachable

switch_case_constant_2:                           ; preds = %contract_call_join_block
  %comparison_result56 = icmp ugt i256 %_383, 18446744073709551615
  br i1 %comparison_result56, label %if_main59, label %if_join60

if_main59:                                        ; preds = %switch_case_constant_2
  store i256 35408467139433450592217433187231851964531694900788300625387963629091585785856, i256 addrspace(1)* null, align 536870912
  store i256 65, i256 addrspace(1)* inttoptr (i256 4 to i256 addrspace(1)*), align 4
  call void @llvm.syncvm.revert(i256 664082786653543858176) #1
  unreachable

if_join60:                                        ; preds = %switch_case_constant_2
  %memory_load_result = load i256, i256 addrspace(1)* inttoptr (i256 64 to i256 addrspace(1)*), align 64
  %addition_result63 = add nuw nsw i256 %_383, 31
  %and_result65 = and i256 %addition_result63, -32
  %addition_result66 = add nuw nsw i256 %and_result65, 63
  %and_result68 = and i256 %addition_result66, -32
  %addition_result69 = add i256 %memory_load_result, %and_result68
  %comparison_result72 = icmp ugt i256 %addition_result69, 18446744073709551615
  %comparison_result76 = icmp ult i256 %addition_result69, %memory_load_result
  %or_result94 = or i1 %comparison_result72, %comparison_result76
  br i1 %or_result94, label %if_main79, label %if_join80

if_main79:                                        ; preds = %if_join60
  store i256 35408467139433450592217433187231851964531694900788300625387963629091585785856, i256 addrspace(1)* null, align 536870912
  store i256 65, i256 addrspace(1)* inttoptr (i256 4 to i256 addrspace(1)*), align 4
  call void @llvm.syncvm.revert(i256 664082786653543858176) #1
  unreachable

if_join80:                                        ; preds = %if_join60
  store i256 %addition_result69, i256 addrspace(1)* inttoptr (i256 64 to i256 addrspace(1)*), align 64
  %memory_store_pointer84 = inttoptr i256 %memory_load_result to i256 addrspace(1)*
  store i256 %_383, i256 addrspace(1)* %memory_store_pointer84, align 1
  %addition_result87 = add i256 %memory_load_result, 32
  %return_data_value89 = load i256, i256 addrspace(1)* inttoptr (i256 16777088 to i256 addrspace(1)*), align 128
  %return_data_copy_destination_pointer = inttoptr i256 %addition_result87 to i256 addrspace(1)*
  %return_data_copy_child_offset = load i256, i256 addrspace(1)* inttoptr (i256 16777120 to i256 addrspace(1)*), align 32
  %return_data_copy_source_pointer = inttoptr i256 %return_data_copy_child_offset to i256 addrspace(3)*
  call void @llvm.memcpy.p1i256.p3i256.i256(i256 addrspace(1)* align 1 %return_data_copy_destination_pointer, i256 addrspace(3)* align 1 %return_data_copy_source_pointer, i256 %return_data_value89, i1 false)
  %phi.bo106 = and i256 %addition_result87, 18446744073709551615
  %phi.bo107 = or i256 %phi.bo106, 1180591620717411303424
  br label %switch_join
}

; Function Attrs: nounwind
declare i256 @llvm.syncvm.getu128() #1

; Function Attrs: noreturn nounwind
declare void @llvm.syncvm.revert(i256) #2

; Function Attrs: argmemonly mustprogress nofree nounwind willreturn
declare void @llvm.memcpy.p1i256.p2i256.i256(i256 addrspace(1)* noalias nocapture writeonly, i256 addrspace(2)* noalias nocapture readonly, i256, i1 immarg) #3

; Function Attrs: noreturn nounwind
declare void @llvm.syncvm.return(i256) #2

; Function Attrs: nofree nosync nounwind readnone
declare i256 @llvm.syncvm.this() #4

; Function Attrs: nounwind
declare i256 @llvm.syncvm.ergsleft() #1

; Function Attrs: argmemonly mustprogress nofree nounwind willreturn
declare void @llvm.memcpy.p1i256.p3i256.i256(i256 addrspace(1)* noalias nocapture writeonly, i256 addrspace(3)* noalias nocapture readonly, i256, i1 immarg) #3

attributes #0 = { noreturn }
attributes #1 = { nounwind }
attributes #2 = { noreturn nounwind }
attributes #3 = { argmemonly mustprogress nofree nounwind willreturn }
attributes #4 = { nofree nosync nounwind readnone }


