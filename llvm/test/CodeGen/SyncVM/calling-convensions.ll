; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "tvm"

; CHECK-LABEL: caller1
define i256 @caller1(i256 %a1) nounwind {
; CHECK: call onearg
  %1 = call i256 @onearg(i256 %a1)
  ret i256 %1
}

; CHECK-LABEL: caller2
define i256 @caller2(i256 %a1, i256 %a2) nounwind {
; CHECK: call twoarg
  %1 = call i256 @twoarg(i256 %a1, i256 %a2)
  ret i256 %1
}

; CHECK-LABEL: caller2.swp
define i256 @caller2.swp(i256 %a1, i256 %a2) nounwind {
; CHECK: add	r1, r0, r3
; CHECK: add	r2, r0, r1
; CHECK: add	r3, r0, r2
; CHECK: call twoarg
  %1 = call i256 @twoarg(i256 %a2, i256 %a1)
  ret i256 %1
}

; CHECK-LABEL: caller3
define i256 @caller3(i256 %a1, i256 %a2, i256 %a3) nounwind {
; CHECK: call threearg
  %1 = call i256 @threearg(i256 %a1, i256 %a2, i256 %a3)
  ret i256 %1
}

; CHECK-LABEL: caller_argtypes
define i256 @caller_argtypes(i1 %a1, i8 %a2, i16 %a3, i32 %a4, i64 %a5, i128 %a6) nounwind {
  %1 = call i256 @i1.arg(i1 %a1)
  %2 = call i256 @i8.arg(i8 %a2)
  %3 = call i256 @i16.arg(i16 %a3)
  %4 = call i256 @i32.arg(i32 %a4)
  %5 = call i256 @i64.arg(i64 %a5)
  %6 = call i256 @i128.arg(i128 %a6)

  ret i256 %6
}

; CHECK: @caller_i1.ret
define i256 @caller_i1.ret(i256 %a1) nounwind {
; CHECK: call	i1.ret
  %1 = call i1 @i1.ret(i256 %a1)
; CHECK: movl #2, r2
; CHECK: movh #0, r2
; CHECK: div r1, r2, r0, r1
  %2 = zext i1 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i8.ret
define i256 @caller_i8.ret(i256 %a1) nounwind {
; CHECK: call	i8.ret
  %1 = call i8 @i8.ret(i256 %a1)
; CHECK: movl #256, r2
; CHECK: movh #0, r2
; CHECK: div r1, r2, r0, r1
  %2 = zext i8 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i16.ret
define i256 @caller_i16.ret(i256 %a1) nounwind {
; CHECK: call	i16.ret
  %1 = call i16 @i16.ret(i256 %a1)
; CHECK: movl #65536, r2
; CHECK: movh #0, r2
; CHECK: div r1, r2, r0, r1
  %2 = zext i16 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i32.ret
define i256 @caller_i32.ret(i256 %a1) nounwind {
; CHECK: call	i32.ret
  %1 = call i32 @i32.ret(i256 %a1)
; CHECK: movl #4294967296, r2
; CHECK: movh #0, r2
; CHECK: div r1, r2, r0, r1
  %2 = zext i32 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i64.ret
define i256 @caller_i64.ret(i256 %a1) nounwind {
; CHECK: call	i64.ret
  %1 = call i64 @i64.ret(i256 %a1)
; CHECK: movl #18446744073709551616, r2
; CHECK: movh #0, r2
; CHECK: div r1, r2, r0, r1
  %2 = zext i64 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i128.ret
define i256 @caller_i128.ret(i256 %a1) nounwind {
; CHECK: call	i128.ret
  %1 = call i128 @i128.ret(i256 %a1)
; CHECK: movl #0, r2
; CHECK: movh #1, r2
; CHECK: div r1, r2, r0, r1
  %2 = zext i128 %1 to i256
  ret i256 %2
}

declare i256 @i1.arg(i1 %a1) nounwind
declare i256 @i8.arg(i8 %a1) nounwind
declare i256 @i16.arg(i16 %a1) nounwind
declare i256 @i32.arg(i32 %a1) nounwind
declare i256 @i64.arg(i64 %a1) nounwind
declare i256 @i128.arg(i128 %a1) nounwind

declare i1 @i1.ret(i256 %a1) nounwind
declare i8 @i8.ret(i256 %a1) nounwind
declare i16 @i16.ret(i256 %a1) nounwind
declare i32 @i32.ret(i256 %a1) nounwind
declare i64 @i64.ret(i256 %a1) nounwind
declare i128 @i128.ret(i256 %a1) nounwind

declare i256 @onearg(i256 %a1) nounwind
declare i256 @twoarg(i256 %a1, i256 %a2) nounwind
declare i256 @threearg(i256 %a1, i256 %a2, i256 %a3) nounwind
