; RUN: llc --evm-keep-registers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define void @sstore(ptr addrspace(5) %key, i256 %val) nounwind {
; CHECK-LABEL: @sstore
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: SSTORE [[IN1]], [[IN2]]

  store i256 %val, ptr addrspace(5) %key, align 32
  ret void
}

define i256 @sload(ptr addrspace(5) %key) nounwind {
; CHECK-LABEL: @sload
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: SLOAD [[RES1:\$[0-9]+]], [[IN1]]

  %val = load i256, ptr addrspace(5) %key, align 32
  ret i256 %val
}
