; RUN: llc < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32"
target triple = "eravm"

; CHECK-LABEL: test
; CHECK: add r2, r0, stack-[1]
; CHECK:  near_call r0, @foo, @.BB0_1
; CHECK: ret
; CHECK: .BB0_1:
; CHECK: add stack-[1], r0, r1
; CHECK: revert
define void @test(i32 %a, i256 %throwval) personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  invoke void @foo(i32 %a)
          to label %try.cont unwind label %lpad
lpad:
  %0 = landingpad { i8*, i32 } catch i8* null
  call void @llvm.eravm.throw(i256 %throwval)
  unreachable
try.cont:
  ret void
}

; CHECK-LABEL: test2
; CHECK: add r2, r0, stack-[3]
; CHECK: near_call r0, @foo16, @.BB1_1
; CHECK: ret
; CHECK: .BB1_1:
; CHECK: add stack-[3], r0, r1
; CHECK: revert
define void @test2(i32 %a, i256 %throwval) personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  invoke void @foo16(i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 0, i32 0)
          to label %try.cont unwind label %lpad
lpad:
  %0 = landingpad { i8*, i32 } catch i8* null
  call void @llvm.eravm.throw(i256 %throwval)
  unreachable
try.cont:
  ret void
}

; CHECK-LABEL: test3
; CHECK: near_call r0, @foo16, @.BB2_2
; CHECK: ret
; CHECK: .BB2_2:
; CHECK: add stack-[3], r0, r1
; CHECK: revert
define void @test3(i32 %a, i256 %throwval) personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  %cmp = icmp ugt i32 %a, 42
  br i1 %cmp, label %try.cont, label %invokebb

invokebb:
  invoke void @foo16(i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a)
          to label %try.cont unwind label %lpad
lpad:
  %0 = landingpad { i8*, i32 } catch i8* null
  call void @llvm.eravm.throw(i256 %throwval)
  unreachable
try.cont:
  ret void
}

; CHECK-LABEL: test4
; CHECK: near_call r0, @foo16, @.BB3_2
; CHECK: .BB3_2:
; CHECK: add     r1, r2, r1
; CHECK: ret
define i256 @test4(i32 %a) personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  %cmp = icmp ugt i32 %a, 42
  br i1 %cmp, label %try.cont, label %invokebb

invokebb:
  invoke void @foo16(i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a)
          to label %try.cont unwind label %lpad
lpad:
  %0 = landingpad {i256, i256} catch i8* null
  %1 = extractvalue { i256, i256 } %0, 0
  %2 = extractvalue { i256, i256 } %0, 1
  %x = add i256 %1, %2
  ret i256 %x
try.cont:
  unreachable
}

declare void @foo(i32)
declare void @foo16(i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)
declare i32 @__gxx_personality_v0(...)
declare void @llvm.eravm.throw(i256)
