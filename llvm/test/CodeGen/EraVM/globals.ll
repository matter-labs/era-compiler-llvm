; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = global i256 0
@val2 = global i256 42
@val3 = global i256 0
@val.arr = global [4 x i256] zeroinitializer
@val2.arr = global [4 x i256] [i256 1, i256 2, i256 3, i256 4]
@as3_ptr = global i8 addrspace(3)* zeroinitializer
@val.code.arr = addrspace(4) global [4 x i256] zeroinitializer

; CHECK-LABEL: load.elem
define i256 @load.elem() nounwind {
  ; CHECK: add stack[@val], r0, r1
  %res = load i256, i256* @val
  ret i256 %res
}

; CHECK-LABEL: store.elem
define void @store.elem(i256 %val) nounwind {
  ; CHECK: add r1, r0, stack[@val]
  store i256 %val, i256* @val
  ret void
}

; CHECK-LABEL: load.fromarray
define i256 @load.fromarray(i256 %i) nounwind {
  %elem = getelementptr [4 x i256], [4 x i256]* @val2.arr, i256 0, i256 %i
  ; CHECK: add stack[@val2.arr + r1], r0, r1
  %res = load i256, i256* %elem
  ret i256 %res
}

; CHECK-LABEL: load.fromarray2
define i256 @load.fromarray2(i256 %i) nounwind {
  %elem = getelementptr [4 x i256], [4 x i256]* @val2.arr, i256 0, i256 1
  %elem2 = getelementptr i256, i256* %elem, i256 %i
  ; TODO: CPR-1280: assembler to support this stack access pattern
  ; CHECK: add stack[@val2.arr + 1 + r1], r0, r1
  %res = load i256, i256* %elem2
  ret i256 %res
}

; CHECK-LABEL: store.toarray2
define void @store.toarray2(i256 %i) nounwind {
  %elem = getelementptr [4 x i256], [4 x i256]* @val2.arr, i256 0, i256 1
  %elem2 = getelementptr i256, i256* %elem, i256 %i
  ; CHECK: add 1024, r0, stack[@val2.arr + 1 + r1]
  store i256 1024, i256* %elem2
  ret void
}

; CHECK-LABEL: load.fromcodearray
define i256 @load.fromcodearray(i256 %i) nounwind {
  %elem = getelementptr [4 x i256], [4 x i256] addrspace(4)* @val.code.arr, i256 0, i256 1
  %elem2 = getelementptr i256, i256 addrspace(4)* %elem, i256 %i
; CHECK: add 1, r1, r[[REG:[0-9]+]]
; CHECK: add @val.code.arr[r[[REG]]], r0, r1
  %res = load i256, i256 addrspace(4)* %elem2
  ret i256 %res
}

; CHECK-LABEL: store.toarray
define void @store.toarray(i256 %val, i256 %i) nounwind {
  %elem = getelementptr [4 x i256], [4 x i256]* @val.arr, i256 0, i256 %i
  ; CHECK: add r1, r0, stack[@val.arr + r2]
  store i256 %val, i256* %elem
  ret void
}

; CHECK-LABEL: load.as3
define i8 addrspace(3)* @load.as3() nounwind {
  ; CHECK: ptr.add stack[@as3_ptr], r0, r1
  %res = load i8 addrspace(3)*, i8 addrspace(3)** @as3_ptr
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: store.as3
define void @store.as3(i8 addrspace(3)* %val) nounwind {
  ; CHECK: ptr.add r1, r0, stack[@as3_ptr]
  store i8 addrspace(3)* %val, i8 addrspace(3)** @as3_ptr
  ret void
}
