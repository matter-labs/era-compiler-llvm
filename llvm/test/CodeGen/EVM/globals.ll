; RUN: llc --mtriple=evm < %s | FileCheck %s

%struct.w = type { i32, i256 }

@val = global i256 0
@val2 = global i256 42
@val3 = global i256 0
@val.arr = global [4 x i256] zeroinitializer
@val2.arr = global [4 x i256] [i256 1, i256 2, i256 3, i256 4]
@as_ptr = global ptr zeroinitializer
@w = external dso_local local_unnamed_addr global %struct.w, align 1

define i256 @load.stelem() {
; CHECK-LABEL: load.stelem
; CHECK: CONST_I256 [[ADDR:\$[0-9]+]], @w+32
; CHECK: MLOAD [[TMP:\$[0-9]+]], [[ADDR]]

  %elem = getelementptr inbounds %struct.w, ptr @w, i32 0, i32 1
  %load = load i256, ptr %elem
  ret i256 %load
}

define i256 @load.elem() nounwind {
; CHECK-LABEL: load.elem
; CHECK: CONST_I256 [[ADDR:\$[0-9]+]], @val
; CHECK: MLOAD [[TMP:\$[0-9]+]], [[ADDR]]

  %res = load i256, i256* @val
  ret i256 %res
}

define void @store.elem(i256 %val) nounwind {
; CHECK-LABEL: store.elem
; CHECK: CONST_I256 [[ADDR:\$[0-9]+]], @val
; CHECK: MSTORE [[ADDR]], {{.*}}

  store i256 %val, i256* @val
  ret void
}

define i256 @load.fromarray(i256 %i) nounwind {
; CHECK-LABEL: load.fromarray
; CHECK: ARGUMENT [[IDX:\$[0-9]+]], 0
; CHECK: CONST_I256 [[C:\$[0-9]+]], 5
; CHECK: SHL [[SHL:\$[0-9]+]], [[IDX]], [[C]]
; CHECK: CONST_I256 [[TMP:\$[0-9]+]], @val2.arr
; CHECK: ADD  [[ADDR:\$[0-9]+]], [[TMP]], [[SHL]]
; CHECK: MLOAD [[RES:\$[0-9]+]], [[ADDR]]

  %elem = getelementptr [4 x i256], [4 x i256]* @val2.arr, i256 0, i256 %i
  %res = load i256, i256* %elem
  ret i256 %res
}

define void @store.toarray(i256 %val, i256 %i) nounwind {
; CHECK-LABEL: store.toarray
; CHECK: ARGUMENT [[IDX:\$[0-9]+]], 1
; CHECK: ARGUMENT [[VAL:\$[0-9]+]], 0
; CHECK: CONST_I256 [[C:\$[0-9]+]], 5
; CHECK: SHL [[SHL:\$[0-9]+]], [[IDX]], [[C]]
; CHECK: CONST_I256 [[TMP:\$[0-9]+]], @val.arr
; CHECK: ADD  [[ADDR:\$[0-9]+]], [[TMP]], [[SHL]]
; CHECK: MSTORE [[ADDR]], [[VAL]]

  %elem = getelementptr [4 x i256], [4 x i256]* @val.arr, i256 0, i256 %i
  store i256 %val, i256* %elem
  ret void
}

define ptr @load.ptr() nounwind {
; CHECK-LABEL: load.ptr
; CHECK: CONST_I256 [[ADDR:\$[0-9]+]], @as_ptr
; CHECK: MLOAD [[TMP:\$[0-9]+]], [[ADDR]]

  %res = load ptr, ptr @as_ptr
  ret ptr %res
}

define void @store.ptr(ptr %val) nounwind {
; CHECK-LABEL: store.ptr
; CHECK: CONST_I256 [[ADDR:\$[0-9]+]], @as_ptr
; CHECK: MSTORE [[ADDR]], {{.*}}

  store ptr %val, ptr @as_ptr
  ret void
}
