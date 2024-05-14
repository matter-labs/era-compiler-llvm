; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK-LABEL: tstore_r
define void @tstore_r(i256 %key, i256 %val) {
; CHECK: stt	r1, r2
  %key_ptr = inttoptr i256 %key to ptr addrspace(6)
  store i256 %val, ptr addrspace(6) %key_ptr, align 64
  ret void
}

; CHECK-LABEL: tload_rr
define i256 @tload_rr(i256 %key) {
; CHECK: ldt r1, r1
  %key_ptr = inttoptr i256 %key to ptr addrspace(6)
  %ret = load i256, ptr addrspace(6) %key_ptr, align 64
  ret i256 %ret
}
