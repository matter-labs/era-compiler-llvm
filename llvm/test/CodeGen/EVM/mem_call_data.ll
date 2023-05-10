; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i256 @mload(ptr addrspace(2) %offset) nounwind {
; CHECK-LABEL: @mload
; CHECK: ARGUMENT [[OFF:\$[0-9]+]], 0
; CHECK: CALLDATALOAD [[RES1:\$[0-9]+]], [[OFF]]

  %val = load i256, ptr addrspace(2) %offset, align 32
  ret i256 %val
}
