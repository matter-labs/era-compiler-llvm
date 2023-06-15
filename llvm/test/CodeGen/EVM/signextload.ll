; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @load_signexti8(ptr %ptr) nounwind {
; CHECK-LABEL: @load_signexti8
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 248
; CHECK: SAR [[RES:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i8, ptr %ptr
  %sext = sext i8 %load to i256
  ret i256 %sext
}

define i256 @load_signexti16(ptr %ptr) nounwind {
; CHECK-LABEL: @load_signexti16
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 240
; CHECK: SAR [[RES:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i16, ptr %ptr
  %sext = sext i16 %load to i256
  ret i256 %sext
}

define i256 @load_signexti32(ptr %ptr) nounwind {
; CHECK-LABEL: @load_signexti32
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 224
; CHECK: SAR [[RES:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i32, ptr %ptr
  %sext = sext i32 %load to i256
  ret i256 %sext
}

define i256 @load_signexti64(ptr %ptr) nounwind {
; CHECK-LABEL: @load_signexti64
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 192
; CHECK: SAR [[RES:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i64, ptr %ptr
  %sext = sext i64 %load to i256
  ret i256 %sext
}

define i256 @load_signexti128(ptr %ptr) nounwind {
; CHECK-LABEL: @load_signexti128
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 128
; CHECK: SAR [[RES:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i128, ptr %ptr
  %sext = sext i128 %load to i256
  ret i256 %sext
}
