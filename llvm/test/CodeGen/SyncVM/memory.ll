; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: store_to_parent
define void @store_to_parent(i256 %par) nounwind {
  %1 = inttoptr i256 0 to i256 addrspace(2)*
; CHECK: mov.p r1, 0
  store i256 %par, i256 addrspace(2)* %1, align 32
  ret void
}

; CHECK-LABEL: store_to_child
define void @store_to_child(i256 %par) nounwind {
  %1 = inttoptr i256 0 to i256 addrspace(3)*
; CHECK: mov.c r1, 0
  store i256 %par, i256 addrspace(3)* %1, align 32
  ret void
}

; CHECK-LABEL: store_to_childuk
define void @store_to_childuk(i256 %par) nounwind {
  %1 = inttoptr i256 42 to i256 addrspace(3)*
; CHECK: mov.c 1, r{{[1-9]}}
; CHECK: or r{{[1-9]}}, r{{[1-9]}}, r[[stcuk_hi:[1-9]]]
; CHECK: mov.c r[[stcuk_hi]], 1
; CHECK: mov.c 2, r{{[1-9]}}
; CHECK: or r{{[1-9]}}, r{{[1-9]}}, r[[stcuk_lo:[1-9]]]
; CHECK: mov.c r[[stcuk_lo]], 2
  store i256 %par, i256 addrspace(3)* %1, align 1
  ret void
}

; CHECK-LABEL: store_to_childuu
define void @store_to_childuu(i256 %addr, i256 %par) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(1)*
; CHECK: call __unaligned_store_as0.{{[0-9]}}
  store i256 %par, i256 addrspace(1)* %1, align 1
  ret void
}

; CHECK-LABEL: load_from_parent
define i256 @load_from_parent(i256 %addr) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(2)*
; CHECK: mov.p 0(r2), r1
  %2 = load i256, i256 addrspace(2)* %1, align 32
  ret i256 %2
}

; CHECK-LABEL: load_from_parentuk
define i256 @load_from_parentuk() nounwind {
  %1 = inttoptr i256 42 to i256 addrspace(1)*
; CHECK-DAG: mov 2, r[[lfpuk_lo:[1-9]]]
; CHECK-DAG: shr r[[lfpuk_lo]], #176, r[[lfpuk_los:[1-9]]]
; CHECK-DAG: mov 1, r[[lfpuk_hi:[1-9]]]
; CHECK-DAG: shl r[[lfpuk_hi]], #80, r[[lfpuk_his:[1-9]]]
; CHECK:     or r[[lfpuk_his]], r[[lfpuk_los]], r1
  %2 = load i256, i256 addrspace(1)* %1, align 1
  ret i256 %2
}

; CHECK-LABEL: load_from_child
define i256 @load_from_child(i256 %addr) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(3)*
; CHECK: mov.c 0(r2), r1
  %2 = load i256, i256 addrspace(3)* %1, align 32
  ret i256 %2
}

; CHECK-LABEL: load_from_childuu
define i256 @load_from_childuu(i256 %addr) nounwind {
  %1 = inttoptr i256 %addr to i256 addrspace(3)*
; CHECK: call __unaligned_load_as0.{{[0-9]}}
  %2 = load i256, i256 addrspace(3)* %1, align 1
  ret i256 %2
}

; CHECK-LABEL: heap_sextload
define i256 @heap_sextload(i8 addrspace(1)* align 256 %arg) nounwind {
; CHECK: call __small_load_as0.{{[0-9]}}
; CHECK: shl
; CHECK: and
; CHECK: or
  %1 = load i8, i8 addrspace(1)* %arg, align 1
  %2 = sext i8 %1 to i256
  ret i256 %2
}
