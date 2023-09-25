; RUN: llc < %s | FileCheck %s

target triple = "eravm"
target datalayout = "E-p:256:256-i256:256:256-S32"

; CHECK-LABEL: selrrr
define i256 addrspace(3)* @selrrr(i256 addrspace(3)* %ptr1, i256 addrspace(3)* %ptr2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp eq i256 %v3, %v4
  ; CHECK: ptr.add
  ; CHECK: ptr.add.eq
  %2 = select i1 %1, i256 addrspace(3)* %ptr1, i256 addrspace(3)* %ptr2
  ret i256 addrspace(3)* %2
}

