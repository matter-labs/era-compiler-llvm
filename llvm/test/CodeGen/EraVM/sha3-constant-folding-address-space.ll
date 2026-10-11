; RUN: opt -passes=eravm-sha3-constant-folding -memssa-check-limit=1 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare i256 @__sha3(ptr addrspace(1), i256, i1) #0

; Check that constant stores to address spaces other than heap aren't taken as
; heap stores.
define i256 @test_storage_store(i256 %x) nounwind {
; CHECK-LABEL: @test_storage_store(
; CHECK: call i256 @__sha3(
  store i256 %x, ptr addrspace(1) null, align 1
  store i256 7, ptr addrspace(5) null, align 1
  %hash = call i256 @__sha3(ptr addrspace(1) null, i256 32, i1 true)
  ret i256 %hash
}

define i256 @test_aux_heap_store(i256 %x) nounwind {
; CHECK-LABEL: @test_aux_heap_store(
; CHECK: call i256 @__sha3(
  store i256 %x, ptr addrspace(1) null, align 1
  store i256 7, ptr addrspace(2) null, align 1
  %hash = call i256 @__sha3(ptr addrspace(1) null, i256 32, i1 true)
  ret i256 %hash
}

; Check that stores to address spaces other than heap don't prevent folding.
define i256 @test_storage_store_skipped() nounwind {
; CHECK-LABEL: @test_storage_store_skipped(
; CHECK-NEXT:    store i256 5, ptr addrspace(1) null, align 1
; CHECK-NEXT:    store i256 7, ptr addrspace(5) null, align 1
; CHECK-NEXT:    ret i256 1546678032441257452667456735582814959992782782816731922691272282333561699760
  store i256 5, ptr addrspace(1) null, align 1
  store i256 7, ptr addrspace(5) null, align 1
  %hash = call i256 @__sha3(ptr addrspace(1) null, i256 32, i1 true)
  ret i256 %hash
}

define i256 @test_aux_heap_store_skipped() nounwind {
; CHECK-LABEL: @test_aux_heap_store_skipped(
; CHECK-NEXT:    store i256 5, ptr addrspace(1) null, align 1
; CHECK-NEXT:    store i256 7, ptr addrspace(2) null, align 1
; CHECK-NEXT:    ret i256 1546678032441257452667456735582814959992782782816731922691272282333561699760
  store i256 5, ptr addrspace(1) null, align 1
  store i256 7, ptr addrspace(2) null, align 1
  %hash = call i256 @__sha3(ptr addrspace(1) null, i256 32, i1 true)
  ret i256 %hash
}

attributes #0 = { memory(argmem: read) nofree null_pointer_is_valid }
