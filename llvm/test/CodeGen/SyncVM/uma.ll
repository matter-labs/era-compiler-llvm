; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"


; CHECK-LABEL: uma.load_heap
define i256 @uma.load_heap(i256 addrspace(1)* %ptr) nounwind {
; CHECK: uma.heap_read r1, r0, r1
  %val = load i256, i256 addrspace(1)* %ptr
  ret i256 %val
}

; CHECK-LABEL: uma.load_calldata
define i256 @uma.load_calldata(i256 addrspace(2)* %ptr) nounwind {
; CHECK: uma.calldata_read r1, r0, r1
  %val = load i256, i256 addrspace(2)* %ptr
  ret i256 %val
}

; CHECK-LABEL: uma.load_returndata
define i256 @uma.load_returndata(i256 addrspace(3)* %ptr) nounwind {
; CHECK: uma.returndata_read r1, r0, r1
  %val = load i256, i256 addrspace(3)* %ptr
  ret i256 %val
}

; CHECK-LABEL: uma.store_heap
define void @uma.store_heap(i256 %val, i256 addrspace(1)* %ptr) nounwind {
; CHECK: uma.heap_write r2, r1, r0
  store i256 %val, i256 addrspace(1)* %ptr, align 1
  ret void
}
