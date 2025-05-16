; RUN: opt -passes=instcombine -S  < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK-LABEL: @test
define i256 @test(i8 addrspace(3)* %calldata_ptr, i256 %off1, i256 %off2) nounwind {
; CHECK-NEXT:  [[TMP1:%.*]] = getelementptr i8, ptr addrspace(3) %calldata_ptr, i256 %off1
; CHECK-NEXT:  [[CALLDATA_PTR_WITH_OFF:%.*]] = getelementptr i8, ptr addrspace(3) [[TMP1]], i256 %off2
; CHECK-NEXT:  [[RES:%.*]] = load i256, ptr addrspace(3) [[CALLDATA_PTR_WITH_OFF]], align 32
; CHECK-NEXT:  ret i256 [[RES]]
;
  %off = add i256 %off1, %off2
  %calldata_ptr_with_off = getelementptr i8, ptr addrspace(3) %calldata_ptr, i256 %off
  %res = load i256, ptr addrspace(3) %calldata_ptr_with_off, align 32
  ret i256 %res
}
