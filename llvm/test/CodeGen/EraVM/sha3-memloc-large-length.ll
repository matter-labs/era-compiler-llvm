; RUN: opt -aa-pipeline=basic-aa -passes=aa-eval -print-all-alias-modref-info -disable-output < %s 2>&1 | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare i256 @__sha3(ptr addrspace(1), i256, i1) #0

; Check that a length that doesn't fit 64 bits isn't truncated.
; CHECK-LABEL: Function: test
; CHECK: Just Ref: Ptr: i256* %q <-> %h = call i256 @__sha3(ptr addrspace(1) %p, i256 18446744073709551648, i1 true)
define i256 @test(ptr addrspace(1) %p) {
  %q = getelementptr i8, ptr addrspace(1) %p, i256 64
  store i256 1, ptr addrspace(1) %q, align 32
  %h = call i256 @__sha3(ptr addrspace(1) %p, i256 18446744073709551648, i1 true)
  ret i256 %h
}

attributes #0 = { memory(argmem: read) nofree null_pointer_is_valid }
