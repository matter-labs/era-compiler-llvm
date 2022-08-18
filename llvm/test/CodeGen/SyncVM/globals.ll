; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = global i256 0
@val2 = global i256 42
@val3 = global i256 0
@val.arr = global [4 x i256] zeroinitializer
@val2.arr = global [4 x i256] [i256 1, i256 2, i256 3, i256 4]
@as3_ptr = global i8 addrspace(3)* zeroinitializer

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
  ; CHECK: add stack[r1 + @val2.arr], r0, r1
  %res = load i256, i256* %elem 
  ret i256 %res
}

; CHECK-LABEL: store.toarray
define void @store.toarray(i256 %val, i256 %i) nounwind {
  %elem = getelementptr [4 x i256], [4 x i256]* @val.arr, i256 0, i256 %i
  ; CHECK: add r1, r0, stack[r2 + @val.arr]
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
