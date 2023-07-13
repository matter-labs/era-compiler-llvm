; RUN: llc --mtriple=evm < %s | FileCheck %s

define void @mstore8(ptr addrspace(1) %offset, i256 %val) nounwind {
; CHECK-LABEL: @mstore8
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: MSTORE8 [[IN1]], [[IN2]]

  call void @llvm.evm.mstore8(ptr addrspace(1) %offset, i256 %val)
  ret void
}

define void @mstore(ptr addrspace(1) %offset, i256 %val) nounwind {
; CHECK-LABEL: @mstore
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: MSTORE [[IN1]], [[IN2]]

  store i256 %val, ptr addrspace(1) %offset, align 32
  ret void
}

define i256 @mload(ptr addrspace(1) %offset) nounwind {
; CHECK-LABEL: @mload
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: MLOAD [[RES1:\$[0-9]+]], [[IN1]]

  %val = load i256, ptr addrspace(1) %offset, align 32
  ret i256 %val
}

declare void @llvm.evm.mstore8(ptr addrspace(1), i256)
