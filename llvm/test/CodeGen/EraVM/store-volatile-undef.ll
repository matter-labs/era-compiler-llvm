; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; Volatile stores of undef and poison must be kept, as optimizations must not
; change the number of volatile operations. Non-volatile ones can be removed.

define void @stack(ptr %p) {
; CHECK-LABEL: stack:
; CHECK:       add r{{[0-9]+}}, r0, stack[r1]
; CHECK-NEXT:  ret
  store volatile i256 undef, ptr %p
  ret void
}

define void @heap(ptr addrspace(1) %p) {
; CHECK-LABEL: heap:
; CHECK:       stm.h r1, r{{[0-9]+}}
; CHECK-NEXT:  ret
  store volatile i256 undef, ptr addrspace(1) %p
  ret void
}

define void @heap_poison(ptr addrspace(1) %p) {
; CHECK-LABEL: heap_poison:
; CHECK:       stm.h r1, r{{[0-9]+}}
; CHECK-NEXT:  ret
  store volatile i256 poison, ptr addrspace(1) %p
  ret void
}

define void @aux_heap(ptr addrspace(2) %p) {
; CHECK-LABEL: aux_heap:
; CHECK:       stm.ah r1, r{{[0-9]+}}
; CHECK-NEXT:  ret
  store volatile i256 undef, ptr addrspace(2) %p
  ret void
}

define void @heap_i8(ptr addrspace(1) %p) {
; CHECK-LABEL: heap_i8:
; CHECK:       stm.h r1, r{{[0-9]+}}
; CHECK-NEXT:  ret
  store volatile i8 undef, ptr addrspace(1) %p
  ret void
}

define void @heap_non_volatile(ptr addrspace(1) %p) {
; CHECK-LABEL: heap_non_volatile:
; CHECK-NOT:   stm
; CHECK:       ret
  store i256 undef, ptr addrspace(1) %p
  ret void
}
