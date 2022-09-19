; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; Calls and invokes with and w/o abi data

; CHECK-LABEL: call1.i256
define i256 @call1.i256(i256 %a1) nounwind {
; CHECK: near_call r0, @onearg, @DEFAULT_UNWIND
  %1 = call i256 @onearg(i256 %a1)
  ret i256 %1
}

; CHECK-LABEL: call1.i256abi
define i256 @call1.i256abi(i256 %a1, i256 %abi_data) nounwind {
  %ptr = bitcast i256(i256)* @onearg to i256*
; CHECK: add r2, r0, r15
; CHECK: near_call r15, @onearg, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 %abi_data, i256 %a1)
  ret i256 %1
}

; CHECK-LABEL: call1.i256p
define i256* @call1.i256p(i256 %a1) nounwind {
; CHECK: near_call r0, @onearg.i256p, @DEFAULT_UNWIND
  %1 = call i256* @onearg.i256p(i256 %a1)
  ret i256* %1
}

; CHECK-LABEL: call1.i256pabi
define i256* @call1.i256pabi(i256 %a1, i256 %abi_data) nounwind {
; CHECK: add r2, r0, r15
; CHECK: near_call r15, @onearg.i256p, @DEFAULT_UNWIND
  %1 = call i256 (i256*, i256, ...) @llvm.syncvm.nearcall(i256* bitcast (i256*(i256)* @onearg.i256p to i256*), i256 %abi_data, i256 %a1)
  %2 = inttoptr i256 %1 to i256*
  ret i256* %2
}

; CHECK-LABEL: call1.void
define void @call1.void(i256 %a1) nounwind {
; CHECK: near_call r0, @onearg.void, @DEFAULT_UNWIND
  call void @onearg.void(i256 %a1)
  ret void
}

; CHECK-LABEL: call1.i256voidabi
define void @call1.i256voidabi(i256 %a1, i256 %abi_data) nounwind {
  %ptr = bitcast void(i256)* @onearg.void to i256*
; CHECK: add r2, r0, r15
; CHECK: near_call r15, @onearg.void, @DEFAULT_UNWIND
  call i256 (i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 %abi_data, i256 %a1)
  ret void
}

; CHECK-LABEL: invoke1.i256
define i256 @invoke1.i256(i256 %a1) personality i256()* @__personality {
; CHECK: near_call r0, @onearg, @.BB6_2
  %res = invoke i256 @onearg(i256 %a1) to label %success unwind label %failure
success:
; CHECK: ret
  ret i256 %res
failure:
; CHECK: ret
  %x = landingpad {i256, i256} cleanup
  %xint = extractvalue {i256, i256} %x, 0
  ret i256 %xint
}

; CHECK-LABEL: invoke1.i256abi
define i256 @invoke1.i256abi(i256 %a1, i256 %abi_data) personality i256()* @__personality {
  %ptr = bitcast i256(i256)* @onearg to i256*
; CHECK: add r2, r0, r15
; CHECK: near_call r15, @onearg, @.BB7_2
  %res = invoke i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 %abi_data, i256 %a1) to label %success unwind label %failure
success:
; CHECK: ret
  ret i256 %res
failure:
; CHECK: ret
  %x = landingpad {i256, i256} cleanup
  %xint = extractvalue {i256, i256} %x, 0
  ret i256 %xint
}

; CHECK-LABEL: caller2
define i256 @caller2(i256 %a1, i256 %a2) nounwind {
; CHECK: near_call r0, @twoarg, @DEFAULT_UNWIND
  %1 = call i256 @twoarg(i256 %a1, i256 %a2)
  ret i256 %1
}

; CHECK-LABEL: caller2.abi
define i256 @caller2.abi(i256 %a1, i256 %a2, i256 %abi) nounwind {
  %ptr = bitcast i256(i256, i256)* @twoarg to i256*
; CHECK: add r3, r0, r15
; CHECK: near_call r15, @twoarg, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 %abi, i256 %a1, i256 %a2)
  ret i256 %1
}

; CHECK-LABEL: caller2.swp
define i256 @caller2.swp(i256 %a1, i256 %a2) nounwind {
; CHECK: add	r1, r0, r3
; CHECK: add	r2, r0, r1
; CHECK: add	r3, r0, r2
; CHECK: near_call r0, @twoarg, @DEFAULT_UNWIND
  %1 = call i256 @twoarg(i256 %a2, i256 %a1)
  ret i256 %1
}

; CHECK-LABEL: caller3
define i256 @caller3(i256 %a1, i256 %a2, i256 %a3) nounwind {
; CHECK: call r0, @threearg, @DEFAULT_UNWIND
  %1 = call i256 @threearg(i256 %a1, i256 %a2, i256 %a3)
  ret i256 %1
}

; CHECK-LABEL: caller3.abi
define i256 @caller3.abi(i256 %a1, i256 %a2, i256 %a3, i256 %abi) nounwind {
  %ptr = bitcast i256(i256, i256, i256)* @threearg to i256*
; CHECK: add r4, r0, r15
; CHECK: call r15, @threearg, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 %abi, i256 %a1, i256 %a2, i256 %a3)
  ret i256 %1
}

; CHECK-LABEL: caller_argtypes
define i256 @caller_argtypes(i1 %a1, i8 %a2, i16 %a3, i32 %a4, i64 %a5, i128 %a6) nounwind {
  ; CHECK: nop stack+=[7]
  ; CHECK: add r6, r0, stack-[6]
  ; CHECK: add r5, r0, stack-[5]
  ; CHECK: add r4, r0, stack-[4]
  ; CHECK: add r3, r0, stack-[3]
  ; CHECK: add r2, r0, stack-[2]
  ; CHECK: add r1, r0, stack-[1]

  %ptr1 = bitcast i256(i1)* @i1.arg to i256*
  %ptr8 = bitcast i256(i8)* @i8.arg to i256*
  %ptr16 = bitcast i256(i16)* @i16.arg to i256*
  %ptr32 = bitcast i256(i32)* @i32.arg to i256*
  %ptr64 = bitcast i256(i64)* @i64.arg to i256*
  %ptr128 = bitcast i256(i128)* @i128.arg to i256*

	; CHECK: near_call r0, @i1.arg, @DEFAULT_UNWIND
  %1 = call i256 @i1.arg(i1 %a1)
	; CHECK: add stack-[2], 0, r1
	; CHECK: near_call r0, @i8.arg, @DEFAULT_UNWIND
  %2 = call i256 @i8.arg(i8 %a2)
	; CHECK: add stack-[3], 0, r1
	; CHECK: near_call r0, @i16.arg, @DEFAULT_UNWIND
  %3 = call i256 @i16.arg(i16 %a3)
	; CHECK: add stack-[4], 0, r1
	; CHECK: near_call r0, @i32.arg, @DEFAULT_UNWIND
  %4 = call i256 @i32.arg(i32 %a4)
	; CHECK: add stack-[5], 0, r1
	; CHECK: near_call r0, @i64.arg, @DEFAULT_UNWIND
  %5 = call i256 @i64.arg(i64 %a5)
	; CHECK: add stack-[6], 0, r1
	; CHECK: near_call r0, @i128.arg, @DEFAULT_UNWIND
  %6 = call i256 @i128.arg(i128 %a6)

	; CHECK: add 42, r0, r15
	; CHECK: add r15, r0, stack-[7]
	; CHECK: add stack-[1], 0, r1
	; CHECK: near_call r15, @i1.arg, @DEFAULT_UNWIND
  %7 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr1, i256 42, i1 %a1)
	; CHECK: add stack-[2], 0, r1
	; CHECK: add stack-[7], 0, r15
	; CHECK: near_call r15, @i8.arg, @DEFAULT_UNWIND
  %8 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr8, i256 42, i8 %a2)
	; CHECK: add stack-[3], 0, r1
	; CHECK: add stack-[7], 0, r15
	; CHECK: near_call r15, @i16.arg, @DEFAULT_UNWIND
  %9 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr16, i256 42, i16 %a3)
	; CHECK: add stack-[4], 0, r1
	; CHECK: add stack-[7], 0, r15
	; CHECK: near_call r15, @i32.arg, @DEFAULT_UNWIND
  %10 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr32, i256 42, i32 %a4)
	; CHECK: add stack-[5], 0, r1
	; CHECK: add stack-[7], 0, r15
	; CHECK: near_call r15, @i64.arg, @DEFAULT_UNWIND
  %11 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr64, i256 42, i64 %a5)
	; CHECK: add stack-[6], 0, r1
	; CHECK: add stack-[7], 0, r15
	; CHECK: near_call r15, @i128.arg, @DEFAULT_UNWIND
  %12 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr128, i256 42, i128 %a6)

  ret i256 %12
}

; CHECK: @caller_i1.ret
define i256 @caller_i1.ret(i256 %a1) nounwind {
; CHECK: near_call r0, @i1.ret, @DEFAULT_UNWIND
  %1 = call i1 @i1.ret(i256 %a1)
; CHECK-NOT: and 1, r1, r1
  %2 = zext i1 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i1.retabi
define i256 @caller_i1.retabi(i256 %a1) nounwind {
  %ptr = bitcast i1(i256)* @i1.ret to i256*
; CHECK: near_call r15, @i1.ret, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 42, i256 %a1)
  ret i256 %1
}

; CHECK: @caller_i8.ret
define i256 @caller_i8.ret(i256 %a1) nounwind {
; CHECK: near_call r0, @i8.ret, @DEFAULT_UNWIND
  %1 = call i8 @i8.ret(i256 %a1)
; CHECK-NOT: and 255, r1, r1
  %2 = zext i8 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i8.retabi
define i256 @caller_i8.retabi(i256 %a1) nounwind {
  %ptr = bitcast i8(i256)* @i8.ret to i256*
; CHECK: near_call r15, @i8.ret, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 42, i256 %a1)
  ret i256 %1
}

; CHECK: @caller_i16.ret
define i256 @caller_i16.ret(i256 %a1) nounwind {
; CHECK: near_call r0, @i16.ret, @DEFAULT_UNWIND
  %1 = call i16 @i16.ret(i256 %a1)
; CHECK-NOT: and 65535, r1, r1
  %2 = zext i16 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i16.retabi
define i256 @caller_i16.retabi(i256 %a1) nounwind {
  %ptr = bitcast i16(i256)* @i16.ret to i256*
; CHECK: near_call r15, @i16.ret, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 42, i256 %a1)
  ret i256 %1
}

; CHECK: @caller_i32.ret
define i256 @caller_i32.ret(i256 %a1) nounwind {
; CHECK: near_call r0, @i32.ret, @DEFAULT_UNWIND
  %1 = call i32 @i32.ret(i256 %a1)
  %2 = zext i32 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i32.retabi
define i256 @caller_i32.retabi(i256 %a1) nounwind {
  %ptr = bitcast i32(i256)* @i32.ret to i256*
; CHECK: near_call r15, @i32.ret, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 42, i256 %a1)
  ret i256 %1
}

; CHECK: @caller_i64.ret
define i256 @caller_i64.ret(i256 %a1) nounwind {
; CHECK: near_call r0, @i64.ret, @DEFAULT_UNWIND
  %1 = call i64 @i64.ret(i256 %a1)
  %2 = zext i64 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i64.retabi
define i256 @caller_i64.retabi(i256 %a1) nounwind {
  %ptr = bitcast i64(i256)* @i64.ret to i256*
; CHECK: near_call r15, @i64.ret, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 42, i256 %a1)
  ret i256 %1
}

; CHECK: @caller_i128.ret
define i256 @caller_i128.ret(i256 %a1) nounwind {
; CHECK: near_call r0, @i128.ret
  %1 = call i128 @i128.ret(i256 %a1)
  %2 = zext i128 %1 to i256
  ret i256 %2
}

; CHECK: @caller_i128.retabi
define i256 @caller_i128.retabi(i256 %a1) nounwind {
  %ptr = bitcast i128(i256)* @i128.ret to i256*
; CHECK: near_call r15, @i128.ret, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 42, i256 %a1)
  ret i256 %1
}

; CHECK-LABEL: call.onestack
define i256 @call.onestack() nounwind {
; TODO: Check calling conventions onse callee-saved and caller-saver registers defined
; CHECK-NOT: nop stack+=[{0-9}]
; CHECK: context.sp r1
; CHECK: add 0, r0, stack[r1 - 0]
; CHECK: add r0, r0, r1
; CHECK: add r0, r0, r2
; CHECK: add r0, r0, r3
; CHECK: add r0, r0, r4
; CHECK: add r0, r0, r5
; CHECK: add r0, r0, r6
; CHECK: add r0, r0, r7
; CHECK: add r0, r0, r8
; CHECK: add r0, r0, r9
; CHECK: add r0, r0, r10
; CHECK: add r0, r0, r11
; CHECK: add r0, r0, r12
; CHECK: add r0, r0, r13
; CHECK: add r0, r0, r14
; CHECK: near_call r0, @onestack
  %1 = call i256 @onestack(i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0)
; CHECK-NOT: nop stack-=[{0-9}]
  ret i256 %1
}


; CHECK-LABEL: call.onestackabi
define i256 @call.onestackabi() nounwind {
  %ptr = bitcast i256(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256)* @onestack to i256*
; CHECK-NOT: nop stack+=[{0-9}]
; CHECK: context.sp r1
; CHECK: add 0, r0, stack[r1 - 0]
; CHECK: add 42, r0, r15
; CHECK: add r0, r0, r2
; CHECK: add r0, r0, r3
; CHECK: add r0, r0, r4
; CHECK: add r0, r0, r5
; CHECK: add r0, r0, r6
; CHECK: add r0, r0, r7
; CHECK: add r0, r0, r8
; CHECK: add r0, r0, r9
; CHECK: add r0, r0, r10
; CHECK: add r0, r0, r11
; CHECK: add r0, r0, r12
; CHECK: add r0, r0, r13
; CHECK: add r0, r0, r14
; CHECK: near_call r15, @onestack, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 42, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0)
; CHECK-NOT: nop stack-=[{0-9}]
  ret i256 %1
}

; CHECK-LABEL: call.twostackabi
define i256 @call.twostackabi() nounwind {
  %ptr = bitcast i256(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256)* @twostack to i256*
; CHECK: context.sp      r1
; CHECK: add 2, r0, stack[r1 + 1]
; CHECK: add 1, r0, stack[r1 - 0]
; CHECK: add 42, r0, r15
; CHECK: add r0, r0, r2
; CHECK: add r0, r0, r3
; CHECK: add r0, r0, r4
; CHECK: add r0, r0, r5
; CHECK: add r0, r0, r6
; CHECK: add r0, r0, r7
; CHECK: add r0, r0, r8
; CHECK: add r0, r0, r9
; CHECK: add r0, r0, r10
; CHECK: add r0, r0, r11
; CHECK: add r0, r0, r12
; CHECK: add r0, r0, r13
; CHECK: add r0, r0, r14
; CHECK: near_call r15, @twostack, @DEFAULT_UNWIND
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %ptr, i256 42, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 0, i256 1, i256 2)
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
; CHECK: add stack-[3], r1, r1
; CHECK: add stack-[2], r1, r1
; CHECK: add stack-[1], r1, r1
  %14 = add i256 %13, %a15
  %15 = add i256 %14, %a16
  %16 = add i256 %15, %a17
  ret i256 %16
}

; CHECK-LABEL: checkcc
define void @checkcc(i256 %arg) nounwind {
; CHECK: near_call r0, @ccc, @DEFAULT_UNWIND
  call void @ccc(i256 %arg)
; CHECK: near_call r0, @fastcc, @DEFAULT_UNWIND
  call void @fastcc(i256 %arg)
; CHECK: near_call r0, @coldcc, @DEFAULT_UNWIND
  call void @coldcc(i256 %arg)
  ret void
}

; CHECK-LABEL: @callee_small_argtypes
define i256 @callee_small_argtypes(i1 %a1, i2 %a2, i8 %a8, i16 %a16, i32 %a32, i64 %a64, i128 %a128, i256 %a256, i42 %a42) nounwind {
  %a1_ = zext i1 %a1 to i256
; CHECK-NOT: and 1, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NOT: and 3, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NOT: and 255, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NOT: and 65535, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NOT: and r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NOT: and r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %a8_ = zext i8 %a8 to i256
  %a16_ = zext i16 %a16 to i256
  %a32_ = zext i32 %a32 to i256
  %a64_ = zext i64 %a64 to i256
  %a128_ = zext i128 %a128 to i256

; non simple types
  %a2_ = zext i2 %a2 to i256
  %a42_ = zext i42 %a42 to i256

  %x0 = add i256 %a1_, %a2_
  %x1 = add i256 %x0, %a8_
  %x2 = add i256 %x1, %a16_
  %x3 = add i256 %x2, %a32_
  %x4 = add i256 %x3, %a64_
  %x5 = add i256 %x4, %a128_
  %x6 = add i256 %x5, %a256
  %x7 = add i256 %x6, %a42_
  ret i256 %x7
}

; CHECK-LABEL: fat.ptr.arg
define void @fat.ptr.arg(i256 addrspace(3)* %ptr) {
  %slot = alloca i256 addrspace(3)*
  ; CHECK: ptr.add r1, r0, stack-[1]
  store i256 addrspace(3)* %ptr, i256 addrspace(3)** %slot
  ret void
}

; CHECK-LABEL: fat.ptr.call
define void @fat.ptr.call(i256 %a1, i256 addrspace(3)* %ptr) {
  ; CHECK: add r2, r0, r1
  call void @fat.ptr.arg(i256 addrspace(3)* %ptr)
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

declare i256  @onearg(i256 %a1) nounwind
declare void  @onearg.void(i256 %a1) nounwind
declare i256* @onearg.i256p(i256 %a1) nounwind
declare i256  @twoarg(i256 %a1, i256 %a2) nounwind
declare i256  @threearg(i256 %a1, i256 %a2, i256 %a3) nounwind
declare i256  @onestack(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256) nounwind
declare i256  @twostack(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256) nounwind

declare ccc void @ccc(i256 %a1) nounwind
declare fastcc void @fastcc(i256 %a1) nounwind
declare coldcc void @coldcc(i256 %a1) nounwind

declare i256 @__personality()

declare i256 @llvm.syncvm.nearcall(i256*, i256, ...)
