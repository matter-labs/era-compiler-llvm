; RUN: llc --enable-eravm-combine-addressing-mode=false --disable-eravm-scalar-opt-passes < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32"
target triple = "eravm"

; CHECK-LABEL: store_to_frame
define void @store_to_frame(i256 %par) nounwind {
  %1 = alloca i256
; CHECK: add r1, r0, stack-[1]
  store i256 %par, i256* %1
  ret void
}

; CHECK-LABEL: store_to_frame2
define void @store_to_frame2(i256 %par) nounwind {
  %1 = alloca i256
  %2 = alloca i256
; CHECK: add r1, r0, stack-[1]
  store i256 %par, i256* %1
; CHECK: add r1, r0, stack-[2]
  store i256 %par, i256* %2
  ret void
}

; CHECK-LABEL: store_to_frame_select
define void @store_to_frame_select(i256 %par, i1 %flag) nounwind {
  %1 = alloca i256
  %2 = alloca i256
  %3 = select i1 %flag, i256* %1, i256*%2
; get pointer to %1
; CHECK: sp r3
; CHECK: sub.s 1, r3, r3
; CHECK: shl.s 5, r3, r3
; get pointer to %2
; CHECK: sp r4
; CHECK: sub.s 2, r4, r4
; CHECK: shl.s 5, r4, r4
; the select part
; CHECK: sub! r2, r0, r2
; CHECK: add.eq r3, r0, r4
; store
; CHECK: add r1, r0, stack[r2]
  store i256 %par, i256* %3
  ret void
}

; CHECK-LABEL: load_from_frame
define i256 @load_from_frame(i256 %par) nounwind {
  %1 = alloca i256
; CHECK: add r1, r0, stack-[1]
  store i256 %par, i256* %1
  %2 = call i256 @foo()
; CHECK: add stack-[1], r0, r1
  %3 = load i256, i256* %1
  ret i256 %3
}

; CHECK-LABEL: spill
define i256 @spill(i256 %par, i256 %par2) nounwind {
; CHECK: add r2, r0, stack-[1]
; CHECK: add r1, r0, stack-[2]
  %1 = call i256 @foo()
; CHECK: add stack-[1], r0, r2
  %2 = add i256 %par, %1
  %3 = add i256 %par2, %1
; CHECK: add stack-[2], r0, r3
  %4 = add i256 %2, %3
  ret i256 %4
}

; CHECK-LABEL: store_to_frame.i64
define void @store_to_frame.i64(i64 %par) nounwind {
  %1 = alloca i64, align 32
  %2 = alloca i64, align 32
  store i64 %par, i64* %1, align 32
  store i64 %par, i64* %2, align 32
; TODO: CPR-1003
; CHECK: add @CPI5_0[0], r0, r2
; CHECK: and stack-[1], r2, r3
; CHECK: or  r1, r3, stack-[1]
; CHECK: and stack-[2], r2, r2
; CHECK: or  r1, r2, stack-[2]
  ret void
}

; CHECK-LABEL: load_from_frame.i64
define i64 @load_from_frame.i64(i64 %par) nounwind {
  %1 = alloca i64, align 32
; TODO: CPR-1003
; store i64 to stack
; CHECK: add   @CPI6_0[0], r0, r2
; CHECK: and   stack-[1], r2, r2
; CHECK: shl.s 192, r1, r1
; CHECK: or    r1, r2, stack-[1]
  store i64 %par, i64* %1, align 32
  %2 = call i256 @foo()
; load i64 from stack
; CHECK: add 192, r0, r1
; CHECK: shr stack-[1], r1, r1
  %3 = load i64, i64* %1, align 32
  ret i64 %3
}

declare i256 @foo()
