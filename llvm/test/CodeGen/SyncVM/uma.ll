; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"


; CHECK-LABEL: uma.load_heap
define i256 @uma.load_heap(i256 addrspace(1)* %ptr) nounwind {
; CHECK: ld.1 r1, r1
  %val = load i256, i256 addrspace(1)* %ptr
  ret i256 %val
}

; CHECK-LABEL: uma.load_heapaux
define i256 @uma.load_heapaux(i256 addrspace(2)* %ptr) nounwind {
; CHECK: ld.2 r1, r1
  %val = load i256, i256 addrspace(2)* %ptr
  ret i256 %val
}

; CHECK-LABEL: uma.load_generic
define i256 @uma.load_generic(i256 addrspace(3)* %ptr) nounwind {
; CHECK: ld r1, r1
  %val = load i256, i256 addrspace(3)* %ptr
  ret i256 %val
}

; CHECK-LABEL: uma.store_heap
define void @uma.store_heap(i256 %val, i256 addrspace(1)* %ptr) nounwind {
; CHECK: st.1 r2, r1
  store i256 %val, i256 addrspace(1)* %ptr, align 1
  ret void
}

; CHECK-LABEL: uma.store_heapaux
define void @uma.store_heapaux(i256 %val, i256 addrspace(2)* %ptr) nounwind {
; CHECK: st.2 r2, r1
  store i256 %val, i256 addrspace(2)* %ptr, align 1
  ret void
}
