; RUN: llc -O3 -stop-after=gvn -compile-twice=false < %s | FileCheck %s --check-prefix=GVN
; RUN: llc -O3 -eravm-enable-new-gvn-passes -stop-after=newgvn -compile-twice=false < %s | FileCheck %s --check-prefix=NEWGVN

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; GVN-LABEL: __entry
; NEWGVN-LABEL: __entry
define noundef i256 @__entry(ptr addrspace(3) %0, i256 %1, i256 %2, i256 %3, i256 %4, i256 %5, i256 %6, i256 %7, i256 %8, i256 %9, i256 %10, i256 %11) local_unnamed_addr #3 personality ptr @__personality {
entry:
  %abi_pointer_value = ptrtoint ptr addrspace(3) %0 to i256
  %abi_pointer_value_shifted = lshr i256 %abi_pointer_value, 96
  %abi_length_value = and i256 %abi_pointer_value_shifted, 4294967295
  %is_deploy_code_call_flag_truncated = and i256 %1, 1
  %is_deploy_code_call_flag.not = icmp eq i256 %is_deploy_code_call_flag_truncated, 0
  store i256 128, ptr addrspace(1) inttoptr (i256 64 to ptr addrspace(1)), align 64
  br i1 %is_deploy_code_call_flag.not, label %runtime_code_call_block, label %deploy_code_call_block

deploy_code_call_block:                           ; preds = %entry
  %get_u128_value.i = tail call i256 @llvm.eravm.getu128()
  %if_condition_compared.not.i = icmp eq i256 %get_u128_value.i, 0
  br i1 %if_condition_compared.not.i, label %if_join.i, label %if_main.i

if_main.i:                                        ; preds = %if_join25.i, %if_main6.i, %runtime_code_call_block, %if_main.i5, %deploy_code_call_block
  tail call void @llvm.eravm.revert(i256 0)
  unreachable

if_join.i:                                        ; preds = %deploy_code_call_block
  store i256 32, ptr addrspace(2) inttoptr (i256 256 to ptr addrspace(2)), align 256
  store i256 0, ptr addrspace(2) inttoptr (i256 288 to ptr addrspace(2)), align 32
  tail call void @llvm.eravm.return(i256 53919893334301279589334030174039261352344891250716429051063678533632)
  unreachable

runtime_code_call_block:                          ; preds = %entry
  %comparison_result.i = icmp ugt i256 %abi_length_value, 3
  br i1 %comparison_result.i, label %if_main.i5, label %if_main.i

if_main.i5:                                       ; preds = %runtime_code_call_block
  %calldata_value.i = load i256, ptr addrspace(3) %0, align 32
  %shift_res.i.mask = and i256 %calldata_value.i, -26959946667150639794667015087019630673637144422540572481103610249216
  %comparison_result3.i = icmp eq i256 %shift_res.i.mask, 5087435675825887412028231804132061875371181388884563200754768051279895724032
  br i1 %comparison_result3.i, label %if_main6.i, label %if_main.i

if_main6.i:                                       ; preds = %if_main.i5
  %get_u128_value.i6 = tail call i256 @llvm.eravm.getu128()
  %if_condition_compared8.not.i = icmp ne i256 %get_u128_value.i6, 0
  %comparison_result12.i = icmp ult i256 %abi_length_value, 36
  %or.cond16 = select i1 %if_condition_compared8.not.i, i1 true, i1 %comparison_result12.i
  br i1 %or.cond16, label %if_main.i, label %if_join16.i

if_join16.i:                                      ; preds = %if_main6.i
  %calldata_pointer_with_offset18.i = getelementptr i8, ptr addrspace(3) %0, i256 4
  %calldata_value19.i = load i256, ptr addrspace(3) %calldata_pointer_with_offset18.i, align 32
  br label %for_condition.i

for_condition.i:                                  ; preds = %if_join25.i, %if_join16.i
; NEWGVN: [[PHI_OF_OPS:%.*]] = phi i1 [ true, %if_join16.i ], [ [[CMP:%.*]], %if_join25.i ]
; NEWGVN: [[CMP]] = icmp ult i256 %var_tokenId27.i12, %calldata_value19.i
; GVN: %or.cond = select i1 %comparison_result21.i, i1 true, i1 %comparison_result28.not.i

  %var_tokenId27.i12 = phi i256 [ %addition_result37.i, %if_join25.i ], [ 0, %if_join16.i ]
  %comparison_result21.i = phi i1 [ false, %if_join25.i ], [ true, %if_join16.i ]
  %comparison_result28.not.i = icmp ult i256 %var_tokenId27.i12, %calldata_value19.i
  %or.cond = select i1 %comparison_result21.i, i1 true, i1 %comparison_result28.not.i
  br i1 %or.cond, label %if_join25.i, label %if_main33.i

if_join25.i:                                      ; preds = %for_condition.i
  %addition_result37.i = add i256 %var_tokenId27.i12, 1
  %memory_load_result.i = load i256, ptr addrspace(1) inttoptr (i256 64 to ptr addrspace(1)), align 64
  %memory_store_pointer.i = inttoptr i256 %memory_load_result.i to ptr addrspace(1)
  store i256 %var_tokenId27.i12, ptr addrspace(1) %memory_store_pointer.i, align 1
  %caller.i = tail call i256 @llvm.eravm.caller()
  %gas_left.i = tail call i256 @llvm.eravm.gasleft()
  %spec.store.select.i = tail call i256 @llvm.umin.i256(i256 %memory_load_result.i, i256 4294967295)
  %spec.store.select46.i = tail call i256 @llvm.umin.i256(i256 %gas_left.i, i256 4294967295)
  %abi_data_input_offset_shifted.i = shl nuw nsw i256 %spec.store.select.i, 64
  %abi_data_gas_shifted.i = shl nuw nsw i256 %spec.store.select46.i, 192
  %abi_data_offset_and_length.i = or disjoint i256 %abi_data_gas_shifted.i, %abi_data_input_offset_shifted.i
  %abi_data_add_system_call_marker.i = or disjoint i256 %abi_data_offset_and_length.i, 904625697166532776746648320380374280103671757735618107014721178055227736064
  %event_writer_call_external.i = tail call { ptr addrspace(3), i1 } @__farcall(i256 %abi_data_add_system_call_marker.i, i256 32781, i256 3, i256 -15402802100530019096323380498944738953123845089667699673314898783681816316945, i256 0, i256 %caller.i, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %event_writer_external_result_status_code_boolean.i = extractvalue { ptr addrspace(3), i1 } %event_writer_call_external.i, 1
  br i1 %event_writer_external_result_status_code_boolean.i, label %for_condition.i, label %if_main.i

if_main33.i:                                      ; preds = %for_condition.i
  %memory_load_result41.i = load i256, ptr addrspace(1) inttoptr (i256 64 to ptr addrspace(1)), align 64
  %memory_store_pointer44.i = inttoptr i256 %memory_load_result41.i to ptr addrspace(1)
  store i256 %var_tokenId27.i12, ptr addrspace(1) %memory_store_pointer44.i, align 1
  %12 = tail call i256 @llvm.umin.i256(i256 %memory_load_result41.i, i256 4294967295)
  %offset_shifted.i.i = shl nuw nsw i256 %12, 64
  %tmp.i.i = or disjoint i256 %offset_shifted.i.i, 2535301200456458802993406410752
  tail call void @llvm.eravm.return(i256 %tmp.i.i)
  unreachable
}

declare void @llvm.eravm.revert(i256) #4
declare i256 @llvm.umin.i256(i256, i256) #5
declare void @llvm.eravm.return(i256) #4
declare i256 @llvm.eravm.caller() #0
declare i256 @llvm.eravm.gasleft() #1
declare i256 @llvm.eravm.getu128() #0
declare i32 @__personality()
declare { ptr addrspace(3), i1 } @__farcall(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256) local_unnamed_addr #2

attributes #0 = { mustprogress nofree nosync nounwind willreturn memory(none) }
attributes #1 = { mustprogress nomerge nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #2 = { nofree null_pointer_is_valid }
attributes #3 = { nofree noreturn null_pointer_is_valid }
attributes #4 = { noreturn nounwind }
attributes #5 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
