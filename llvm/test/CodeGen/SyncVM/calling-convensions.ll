; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

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
; CHECK: and #1, r1, r1
  %2 = zext i1 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i8.ret
define i256 @caller_i8.ret(i256 %a1) nounwind {
; CHECK: call	i8.ret
  %1 = call i8 @i8.ret(i256 %a1)
; CHECK: and #255, r1, r1
  %2 = zext i8 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i16.ret
define i256 @caller_i16.ret(i256 %a1) nounwind {
; CHECK: call	i16.ret
  %1 = call i16 @i16.ret(i256 %a1)
; CHECK: and #65535, r1, r1
  %2 = zext i16 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i32.ret
define i256 @caller_i32.ret(i256 %a1) nounwind {
; CHECK: call	i32.ret
  %1 = call i32 @i32.ret(i256 %a1)
; CHECK: and #4294967295, r1, r1
  %2 = zext i32 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i64.ret
define i256 @caller_i64.ret(i256 %a1) nounwind {
; CHECK: call	i64.ret
  %1 = call i64 @i64.ret(i256 %a1)
; CHECK: and #18446744073709551615, r1, r1
  %2 = zext i64 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i128.ret
define i256 @caller_i128.ret(i256 %a1) nounwind {
; CHECK: call	i128.ret
  %1 = call i128 @i128.ret(i256 %a1)
; CHECK: and #340282366920938463463374607431768211455, r1, r1
  %2 = zext i128 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: call.onestack
define i256 @call.onestack() nounwind {
; CHECK: sfll #0, r2, r2
; CHECK: sflh #0, r2, r2
; CHECK: push #0, r2
; CHECK: add r2, r0, r1
; CHECK: add r2, r0, r3
; CHECK: add r2, r0, r4
; CHECK: add r2, r0, r5
; CHECK: add r2, r0, r6
; CHECK: add r2, r0, r7
; CHECK: add r2, r0, r8
; CHECK: add r2, r0, r9
; CHECK: add r2, r0, r10
; CHECK: add r2, r0, r11
; CHECK: add r2, r0, r12
; CHECK: add r2, r0, r13
; CHECK: add r2, r0, r14
; CHECK: add r2, r0, r15
; CHECK: call onestack
; CHECK: pop #0, r0
  %1 = call i256 @onestack(i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0)
  ret i256 %1
}

; CHECK-LABEL: call.twostack
define i256 @call.twostack() nounwind {
; CHECK: sfll #2, r2, r2
; CHECK: sflh #0, r2, r2
; CHECK: push #0, r2
; CHECK: sfll #1, r2, r2
; CHECK: sflh #0, r2, r2
; CHECK: push #0, r2
; CHECK: sfll #0, r2, r2
; CHECK: sflh #0, r2, r2
; CHECK: add r2, r0, r1
; CHECK: add r2, r0, r3
; CHECK: add r2, r0, r4
; CHECK: add r2, r0, r5
; CHECK: add r2, r0, r6
; CHECK: add r2, r0, r7
; CHECK: add r2, r0, r8
; CHECK: add r2, r0, r9
; CHECK: add r2, r0, r10
; CHECK: add r2, r0, r11
; CHECK: add r2, r0, r12
; CHECK: add r2, r0, r13
; CHECK: add r2, r0, r14
; CHECK: add r2, r0, r15
; CHECK: call twostack
; CHECK: pop #1, r0
  %1 = call i256 @twostack(i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 1, i256 2)
  ret i256 %1
}

; CHECK-LABEL: sum17
define i256 @sum17(i256 %a1, i256 %a2, i256 %a3, i256 %a4, i256 %a5, i256 %a6, i256 %a7, i256 %a8, i256 %a9, i256 %a10, i256 %a11, i256 %a12, i256 %a13, i256 %a14, i256 %a15, i256 %a16, i256 %a17) nounwind {
  %1 = add i256 %a1, %a2
  %2 = add i256 %1, %a3
  %3 = add i256 %2, %a4
  %4 = add i256 %3, %a5
  %5 = add i256 %4, %a6
  %6 = add i256 %5, %a7
  %7 = add i256 %6, %a8
  %8 = add i256 %7, %a9
  %9 = add i256 %8, %a10
  %10 = add i256 %9, %a11
  %11 = add i256 %10, %a12
  %12 = add i256 %11, %a13
  %13 = add i256 %12, %a14
  %14 = add i256 %13, %a15
; CHECK: mov 1(sp), r3
; CHECK: add r2, r3, r2
  %15 = add i256 %14, %a16
; CHECK: mov 2(sp), r3
; CHECK: add r2, r3, r1
  %16 = add i256 %15, %a17
  ret i256 %16
}

; CHECK-LABEL: checkcc
define void @checkcc(i256 %arg) nounwind {
  call void @ccc(i256 %arg)
  call void @fastcc(i256 %arg)
  call void @coldcc(i256 %arg)
  ret void
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
declare i256 @onestack(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256) nounwind
declare i256 @twostack(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256) nounwind

declare ccc void @ccc(i256 %a1) nounwind
declare fastcc void @fastcc(i256 %a1) nounwind
declare coldcc void @coldcc(i256 %a1) nounwind
