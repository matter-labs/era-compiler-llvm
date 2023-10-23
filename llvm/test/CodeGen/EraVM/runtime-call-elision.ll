; RUN: llc -opaque-pointers < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @__small_store_as0(i256 %addr, i256 %size_in_bits, i256 %value)
declare void @__small_store_as1(i256 %addr.i, i256 %value, i256 %size_in_bits)
declare void @__memset_uma_as1(i256 addrspace(1)* %dest, i256 %val, i256 %size)

; CHECK-LABEL: smallstore.as0
define void @smallstore.as0(i256 %addr, i256 %value) {
; CHECK-NOT: __small_store_as0
  call void @__small_store_as0(i256 %addr, i256 0, i256 %value)
  ret void
}

; CHECK-LABEL: smallstore.as1
define void @smallstore.as1(i256 %addr, i256 %value) {
; CHECK-NOT: __small_store_as1
  call void @__small_store_as1(i256 %addr, i256 %value, i256 0)
  ret void
}

; CHECK-LABEL: memset.as1
define void @memset.as1(i256 addrspace(1)* %addr, i256 %value) {
; CHECK-NOT: __memset_uma_as1
  call void @__memset_uma_as1(i256 addrspace(1)* %addr, i256 %value, i256 0)
  ret void
}
