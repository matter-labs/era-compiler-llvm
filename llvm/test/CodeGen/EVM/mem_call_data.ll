; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @mload(ptr addrspace(2) %offset) nounwind {
; CHECK-LABEL: @mload
; CHECK: ARGUMENT [[OFF:\$[0-9]+]], 0
; CHECK: CALLDATALOAD [[RES1:\$[0-9]+]], [[OFF]]

  %val = load i256, ptr addrspace(2) %offset, align 32
  ret i256 %val
}
