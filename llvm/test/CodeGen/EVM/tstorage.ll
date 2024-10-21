; RUN: llc --evm-keep-registers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define void @tstore(ptr addrspace(6) %key, i256 %val) nounwind {
; CHECK-LABEL: @tstore
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: TSTORE [[IN1]], [[IN2]]

  store i256 %val, ptr addrspace(6) %key, align 32
  ret void
}

define i256 @tload(ptr addrspace(6) %key) nounwind {
; CHECK-LABEL: @tload
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: TLOAD [[RES1:\$[0-9]+]], [[IN1]]

  %val = load i256, ptr addrspace(6) %key, align 32
  ret i256 %val
}
