; RUN: opt -passes='early-cse<memssa>' -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test_srequest_noelim1(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_srequest_noelim1(
; CHECK:         call i256 @__system_request
; CHECK-NEXT:    call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %res = add nuw nsw i256 %val1, %val2
  ret i256 %res
}

define i256 @test_srequest_noelim2(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i256 %loop_size, i1 %cond) {
; CHECK-LABEL: @test_srequest_noelim2(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @__system_request
; CHECK:       loop:
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  br i1 %cond, label %loop, label %ret

loop:
  %phi1 = phi i256 [ %sum, %loop ], [ 0, %entry ]
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @dummy()
  %sum = add nuw nsw i256 %phi1, %val2
  %cmp = icmp slt i256 %sum, %loop_size
  br i1 %cmp, label %loop, label %ret

ret:
  %phi2 = phi i256 [ %val1, %entry ], [ %sum, %loop ]
  ret i256 %phi2
}

define i256 @test_sha3_noelim1(ptr addrspace(1) nocapture %addr) {
; CHECK-LABEL: @test_sha3_noelim1(
; CHECK:         call i256 @__sha3
; CHECK-NEXT:    call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %val2 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %res = add nuw nsw i256 %val1, %val2
  ret i256 %res
}

declare void @dummy()
declare i256 @__system_request(i256, i256, i256, ptr) #0
declare i256 @__sha3(i8 addrspace(1)* %0, i256 %1, i1 %throw_at_failure) #0

attributes #0 = { argmemonly nofree null_pointer_is_valid readonly }
