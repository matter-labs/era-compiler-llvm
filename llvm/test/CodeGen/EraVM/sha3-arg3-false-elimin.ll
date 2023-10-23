; RUN: opt -opaque-pointers -O2 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare i256 @__sha3(i8 addrspace(1)* %0, i256 %1, i1 %throw_at_failure) #0

; CHECK-LABEL: @__sha3
; CHECK-NOT: failure_block:
; CHECK-NOT: throw_block:

define i256 @test_true(i8 addrspace(1)* %0, i256 %1) {
  %sha = call i256 @__sha3(i8 addrspace(1)* %0, i256 %1, i1 false) noinline
  ret i256 %sha
}

attributes #0 = { argmemonly readonly nofree null_pointer_is_valid }
