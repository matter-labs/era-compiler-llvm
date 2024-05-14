; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK-LABEL: sstore_r
define void @sstore_r(i256 %key, i256 %val) {
; CHECK: sts r1, r2
  %key_ptr = inttoptr i256 %key to ptr addrspace(5)
  store i256 %val, ptr addrspace(5) %key_ptr, align 64
  ret void
}

; CHECK-LABEL: sload_rr
define i256 @sload_rr(i256 %key) {
; CHECK: lds r1, r1
  %key_ptr = inttoptr i256 %key to ptr addrspace(5)
  %ret = load i256, ptr addrspace(5) %key_ptr, align 64
  ret i256 %ret
}
