; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

@val = internal global i256 0

; CHECK-LABEL: store_to_global
define void @store_to_global(i256 %par) nounwind {
; CHECK: mst r1, @val
  store i256 %par, i256* @val
  ret void
}

; CHECK-LABEL: load_from_global
define i256 @load_from_global() nounwind {
; CHECK: mld @val, r1
  %1 = load i256, i256* @val
  ret i256 %1
}

; CHECK: .type val,@object
; CHECK: .local val
; CHECK: .comm val,32,32
