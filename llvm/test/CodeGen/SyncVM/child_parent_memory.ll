; RUN: llc < %s | FileCheck %s

target datalayout = "S256-e-p:256:256-i256:256:256-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: store_to_child
define void @store_to_child(i256 %par) nounwind {
  %1 = inttoptr i256 0 to i256 addrspace(2)*
; CHECK: movp r1, #0
  store i256 %par, i256 addrspace(2)* %1
  ret void
}

; CHECK-LABEL: store_to_parent
define void @store_to_parent(i256 %par) nounwind {
  %1 = inttoptr i256 0 to i256 addrspace(3)*
; CHECK: movc r1, #0
  store i256 %par, i256 addrspace(3)* %1
  ret void
}

; CHECK-LABEL: load_from_child
define i256 @load_from_child(i256 %addr) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(2)*
; CHECK: movp 0(r1), r1
  %2 = load i256, i256 addrspace(2)* %1
  ret i256 %2
}

; CHECK-LABEL: load_from_parent
define i256 @load_from_parent(i256 %addr) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(3)*
; CHECK: movc 0(r1), r1
  %2 = load i256, i256 addrspace(3)* %1
  ret i256 %2
}
