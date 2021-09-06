; RUN: llc < %s | FileCheck %s

target datalayout = "S256-e-p:256:256-i256:256:256-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: store_to_child
define void @store_to_child(i256 %par) nounwind {
  %1 = inttoptr i256 0 to i256 addrspace(2)*
; CHECK: mov.p r1, 0
  store i256 %par, i256 addrspace(2)* %1
  ret void
}

; CHECK-LABEL: store_to_parent
define void @store_to_parent(i256 %par) nounwind {
  %1 = inttoptr i256 0 to i256 addrspace(3)*
; CHECK: mov.c r1, 0
  store i256 %par, i256 addrspace(3)* %1
  ret void
}

; CHECK-LABEL: load_from_child
define i256 @load_from_child(i256 %addr) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(2)*
; CHECK: mov.p 0(r2), r1
  %2 = load i256, i256 addrspace(2)* %1
  ret i256 %2
}

; CHECK-LABEL: load_from_parent
define i256 @load_from_parent(i256 %addr) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(3)*
; CHECK: mov.c 0(r2), r1
  %2 = load i256, i256 addrspace(3)* %1
  ret i256 %2
}

; CHECK-LABEL: load_from_parent_unaligned_offset
define i256 @load_from_parent_unaligned_offset(i256 %addr) nounwind {
  %aligned = add i256 %addr, 4
  %1 = inttoptr i256 %aligned to i256 addrspace(3)*
; CHECK: add #4, r1, r2
; CHECK: mov.c 0(r2), r1
  %2 = load i256, i256 addrspace(3)* %1
  ret i256 %2
}

; CHECK-LABEL: heap_sextload
define i256 @heap_sextload(i8 addrspace(1)* align 256 %arg) nounwind {
; CHECK: mov
; CHECK: shl
; CHECK: and
  %1 = load i8, i8 addrspace(1)* %arg
  %2 = sext i8 %1 to i256
  ret i256 %2
}
