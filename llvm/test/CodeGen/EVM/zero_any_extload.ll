; RUN: llc --mtriple=evm < %s | FileCheck %s

define i8 @load_anyext_i8(ptr %ptr) nounwind {
; CHECK-LABEL: @load_anyext_i8
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 248
; CHECK: SHR [[SHR:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i8, ptr %ptr
  ret i8 %load
}

define i16 @load_anyext_i16(ptr %ptr) nounwind {
; CHECK-LABEL: @load_anyext_i16
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 240
; CHECK: SHR [[SHR:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i16, ptr %ptr
  ret i16 %load
}

define i32 @load_anyext_i32(ptr %ptr) nounwind {
; CHECK-LABEL: @load_anyext_i32
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 224
; CHECK: SHR [[SHR:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i32, ptr %ptr
  ret i32 %load
}

define i64 @load_anyext_i64(ptr %ptr) nounwind {
; CHECK-LABEL: @load_anyext_i64
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 192
; CHECK: SHR [[SHR:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i64, ptr %ptr
  ret i64 %load
}

define i128 @load_anyext_i128(ptr %ptr) nounwind {
; CHECK-LABEL: @load_anyext_i128
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 128
; CHECK: SHR [[SHR:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i128, ptr %ptr
  ret i128 %load
}

define i256 @load_zeroext_i8(ptr %ptr) nounwind {
; CHECK-LABEL: @load_zeroext_i8
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 248
; CHECK: SHR [[SHR:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i8, ptr %ptr
  %zext = zext i8 %load to i256
  ret i256 %zext
}

define i256 @load_zeroext_i16(ptr %ptr) nounwind {
; CHECK-LABEL: @load_zeroext_i16
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 240
; CHECK: SHR [[SHR:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i16, ptr %ptr
  %zext = zext i16 %load to i256
  ret i256 %zext
}

define i256 @load_zeroext_i32(ptr %ptr) nounwind {
; CHECK-LABEL: @load_zeroext_i32
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 224
; CHECK: SHR [[SHR:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i32, ptr %ptr
  %zext = zext i32 %load to i256
  ret i256 %zext
}

define i256 @load_zeroext_i64(ptr %ptr) nounwind {
; CHECK-LABEL: @load_zeroext_i64
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 192
; CHECK: SHR [[SHR:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i64, ptr %ptr
  %zext = zext i64 %load to i256
  ret i256 %zext
}

define i256 @load_zeroext_i128(ptr %ptr) nounwind {
; CHECK-LABEL: @load_zeroext_i128
; CHECK: MLOAD [[LOAD:\$[0-9]+]], [[PTR:\$[0-9]+]]
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 128
; CHECK: SHR [[SHR:\$[0-9]+]], [[LOAD]], [[EXT]]

  %load = load i128, ptr %ptr
  %zext = zext i128 %load to i256
  ret i256 %zext
}
