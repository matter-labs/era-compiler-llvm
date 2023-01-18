; RUN: llc --verify-coalescing < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@ptr = global i8 addrspace(3)* null

; CHECK-LABEL: test1
define void @test1() personality i32 ()* @__personality {
  %fptr = invoke i8 addrspace(3)* @__farcall_int(i256 0, i256 1)
    to label %ok unwind label %fail
  ; CHECK: far_call	r0, r1, @.BB0_2
  ; CHECK: ptr.add r1, r0, stack-[1]
ok:
  call void @spill()
  ; CHECK: ptr.add stack-[1], r0, r1
  store i8 addrspace(3)* %fptr, i8 addrspace(3)** @ptr
  ret void
fail:
  landingpad {i8 addrspace(3)*, i256} cleanup
  ret void
}

; CHECK-LABEL: test2
define void @test2(i1 %cond) personality i32 ()* @__personality {
  br i1 %cond, label %staticcall, label %delegatecall
staticcall:
  %fptrs = invoke i8 addrspace(3)* @__staticcall_int(i256 0, i256 1)
    to label %ok unwind label %fail
  ; CHECK: far_call.static r0, r1, @.BB1_4
  ; CHECK: ptr.add r1, r0, stack-[1]
delegatecall:
  %fptrd = invoke i8 addrspace(3)* @__delegatecall_int(i256 0, i256 1)
    to label %ok unwind label %fail
  ; CHECK: far_call.delegate r0, r1, @.BB1_4
  ; CHECK: ptr.add r1, r0, stack-[1]
ok:
  %fptr = phi i8 addrspace(3)* [%fptrs, %staticcall], [%fptrd, %delegatecall]
  call void @spill()
  ; CHECK: ptr.add stack-[1], r0, r1
  store i8 addrspace(3)* %fptr, i8 addrspace(3)** @ptr
  ret void
fail:
  landingpad {i8 addrspace(3)*, i256} cleanup
  ret void
}


declare i8 addrspace(3)* @__farcall_int(i256, i256)
declare i8 addrspace(3)* @__staticcall_int(i256, i256)
declare i8 addrspace(3)* @__delegatecall_int(i256, i256)
declare void @spill()
declare i32 @__personality()
