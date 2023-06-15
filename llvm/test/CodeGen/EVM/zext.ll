; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @zexti8(i8 %rs1) nounwind {
; CHECK-LABEL: @zexti8
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 255
; CHECK: AND {{.*}}, {{.*}}, [[EXT]]

  %res = zext i8 %rs1 to i256
  ret i256 %res
}

define i256 @zexti16(i16 %rs1) nounwind {
; CHECK-LABEL: @zexti16
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 65535
; CHECK: AND {{.*}}, {{.*}}, [[EXT]]

  %res = zext i16 %rs1 to i256
  ret i256 %res
}

define i256 @zexti32(i32 %rs1) nounwind {
; CHECK-LABEL: @zexti32
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 4294967295
; CHECK: AND {{.*}}, {{.*}}, [[EXT]]

  %res = zext i32 %rs1 to i256
  ret i256 %res
}

define i256 @zexti64(i64 %rs1) nounwind {
; CHECK-LABEL: @zexti64
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 18446744073709551615
; CHECK: AND {{.*}}, {{.*}}, [[EXT]]

  %res = zext i64 %rs1 to i256
  ret i256 %res
}

define i256 @zexti128(i128 %rs1) nounwind {
; CHECK-LABEL: @zexti128
; CHECK: CONST_I256 [[EXT:\$[0-9]+]], 340282366920938463463374607431768211455
; CHECK: AND {{.*}}, {{.*}}, [[EXT]]

  %res = zext i128 %rs1 to i256
  ret i256 %res
}
