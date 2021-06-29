; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

@val = addrspace(1) global i256 0

; CHECK-LABEL: store_to_global
define void @store_to_global(i256 %par) nounwind {
; CHECK: mov r1, val
  store i256 %par, i256 addrspace(1)* @val
  ret void
}

; CHECK-LABEL: load_from_global
define i256 @load_from_global() nounwind {
; CHECK: mov val, r1
  %1 = load i256, i256 addrspace(1)* @val
  ret i256 %1
}

; CHECK-DAG: .type val,@object
; CHECK-DAG: .globl val
