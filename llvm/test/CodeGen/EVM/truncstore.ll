; RUN: llc --mtriple=evm < %s | FileCheck %s

@glob_i8 = addrspace(1) global i8 0
@glob_i32 = addrspace(1) global i32 0

define void @storei8(i8 %val) nounwind {
; CHECK-LABEL: @storei8
; CHECK: ARGUMENT [[VAL:\$[0-9]+]], 0
; CHECK: CONST_I256 [[ADDR:\$[0-9]+]], @glob_i8
; CHECK: MSTORE8 [[ADDR]], [[VAL]]

  store i8 %val, ptr addrspace(1) @glob_i8
  ret void
}

define void @storei32(i32 %val) nounwind {
; CHECK-LABEL: @storei32
; CHECK: ARGUMENT [[VAL:\$[0-9]+]], 0
; CHECK: CONST_I256 [[TMP1:\$[0-9]+]], 224
; CHECK: SHL [[SHL_VAL:\$[0-9]+]], [[VAL]], [[TMP1]]
; CHECK: CONST_I256 [[ADDR:\$[0-9]+]], @glob_i32
; CHECK: MLOAD [[ORIG_MEM:\$[0-9]+]], [[ADDR]]
; CHECK: CONST_I256 [[TMP2:\$[0-9]+]], 32
; CHECK: SHR [[SHR_MEM:\$[0-9]+]], [[ORIG_MEM]], [[TMP2]]
; CHECK: SHL [[SHL_MEM:\$[0-9]+]], [[SHR_MEM]], [[TMP2]]
; CHECK: OR [[RES_MEM:\$[0-9]+]], [[SHL_VAL]], [[SHL_MEM]]
; CHECK: MSTORE [[ADDR]], [[RES_MEM]]

  store i32 %val, ptr addrspace(1) @glob_i32
  ret void
}
