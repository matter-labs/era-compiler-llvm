; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: store_to_frame
define void @store_to_frame(i256 %par) nounwind {
  %1 = alloca i256
; CHECK: mov r1, 1(sp)
  store i256 %par, i256* %1
  ret void
}

; CHECK-LABEL: store_to_frame2
define void @store_to_frame2(i256 %par) nounwind {
  %1 = alloca i256
  %2 = alloca i256
; CHECK: mov	r1, 1(sp)
  store i256 %par, i256* %1
; CHECK: mov	r1, 2(sp)
  store i256 %par, i256* %2
  ret void
}

; CHECK-LABEL: load_from_frame
define i256 @load_from_frame(i256 %par) nounwind {
  %1 = alloca i256
; CHECK: mov	r1, 1(sp)
  store i256 %par, i256* %1
  %2 = call i256 @foo()
; CHECK: mov 1(sp), r1
  %3 = load i256, i256* %1
  ret i256 %3
}

; CHECK-LABEL: spill
define i256 @spill(i256 %par, i256 %par2) nounwind {
; CHECK: mov r2, 1(sp)
; CHECK: mov r1, 2(sp)
  %1 = call i256 @foo()
; CHECK: mov 1(sp), r2
; CHECK: mov 2(sp), r3
  %2 = add i256 %par, %1
  %3 = add i256 %par2, %1
  %4 = add i256 %2, %3
  ret i256 %4
}

; CHECK-LABEL: store_to_frame.i64
define void @store_to_frame.i64(i64 %par) nounwind {
  %1 = alloca i64, align 32
  %2 = alloca i64, align 32
; CHECK: mov r1, 1(sp)
  store i64 %par, i64* %1
; TODO: Fix truncstores
; CHECK: mov r1, 2(sp)
  store i64 %par, i64* %2
  ret void
}

; CHECK-LABEL: load_from_frame.i64
define i64 @load_from_frame.i64(i64 %par) nounwind {
  %1 = alloca i64, align 32
; CHECK: mov r1, 1(sp)
  store i64 %par, i64* %1
  %2 = call i256 @foo()
; CHECK: mov 1(sp), r1
  %3 = load i64, i64* %1
  ret i64 %3
}

declare i256 @foo()
