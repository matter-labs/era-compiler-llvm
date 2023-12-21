; RUN: opt -passes=eravm-cse -eravm-disable-sha3-sreq-cse -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i1 @test_sha3_nodce(ptr addrspace(1) nocapture %addr) {
; CHECK-LABEL: @test_sha3_nodce(
; CHECK:         call i256 @__sha3
;
entry:
  %val = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  ret i1 true
}

define i1 @test_srequest_nodce(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1) {
; CHECK-LABEL: @test_srequest_nodce(
; CHECK:         call i256 @__system_request
;
entry:
  %val = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  ret i1 true
}

define i256 @test_sha3_noelim(ptr addrspace(1) nocapture %addr) {
; CHECK-LABEL: @test_sha3_noelim(
; CHECK:         call i256 @__sha3
; CHECK-NEXT:    call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %val2 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_srequest_noelim(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1) {
; CHECK-LABEL: @test_srequest_noelim(
; CHECK:         call i256 @__system_request
; CHECK-NEXT:    call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  %res = add i256 %val1, %val2
  ret i256 %res
}

declare i256 @__sha3(i8 addrspace(1)* %0, i256 %1, i1 %throw_at_failure) #0
declare i256 @__system_request(i256, i256, i256, ptr) #0
attributes #0 = { argmemonly nofree null_pointer_is_valid readonly }
