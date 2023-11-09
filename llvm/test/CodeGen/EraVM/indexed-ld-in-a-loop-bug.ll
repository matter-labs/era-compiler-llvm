; RUN: llc --stop-after=eravm-indexed-memops-prepare < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

; CHECK-LABEL: multibb.loop
define void @multibb.loop(ptr addrspace(2) %start, i1 %cond) nounwind {
entry:
  br label %for_body

for_body:                                         ; preds = %if_join31, %entry
  %storemerge49 = phi i256 [ 4, %entry ], [ %addition_result39, %if_join31 ]
  ; CHECK: phi ptr addrspace(2) [ %calldata_pointer_with_offset, %entry ], [ %1, %if_join31 ]
  %calldata_pointer_with_offset = getelementptr i8, ptr addrspace(2) %start, i256 %storemerge49
  %calldata_value = load i256, ptr addrspace(2) %calldata_pointer_with_offset, align 32
  br i1 %cond, label %if_main30, label %if_join31

if_main30:                                        ; preds = %for_body
  unreachable

if_join31:                                        ; preds = %for_body
  store i256 %calldata_value, ptr addrspace(1) undef, align 1
  %addition_result39 = add nuw nsw i256 %storemerge49, 32
  br label %for_body
}
