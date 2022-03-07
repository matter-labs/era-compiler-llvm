; XFAIL: *
; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: store_to_childu
define void @store_to_childu(i256 %addr, i256 %par) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(1)*
; CHECK-DAG: mov.c 0(r{{[1-6]}}), r{{[1-6]}}
; CHECK-DAG: mov.c 1(r{{[1-6]}}), r{{[1-6]}}
; CHECK-DAG: or
; CHECK-DAG: mov.c r{{[1-6]}}, 0(r{{[1-6]}})
; CHECK-DAG: mov.c r{{[1-6]}}, 1(r{{[1-6]}})
  store i256 %par, i256 addrspace(1)* %1, align 1
  ret void
}

; CHECK-LABEL: load_from_parent
define i256 @load_from_parent(i256 %addr) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(2)*
; CHECK: mov.p 0(r2), r1
  %2 = load i256, i256 addrspace(2)* %1
  ret i256 %2
}

; CHECK-LABEL: load_from_parentu
define i256 @load_from_parentu(i256 %addr) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(1)*
; CHECK-DAG: mov.p 0(r{{[1-6]}}), r{{[1-6]}}
; CHECK-DAG: mov.p 1(r{{[1-6]}}), r{{[1-6]}}
; CHECK: or
  %2 = load i256, i256 addrspace(1)* %1, align 1
  ret i256 %2
}

; CHECK-LABEL: load_from_child
define i256 @load_from_child(i256 %addr) nounwind {
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
