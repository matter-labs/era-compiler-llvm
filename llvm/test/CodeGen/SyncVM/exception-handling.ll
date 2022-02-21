; XFAIL: *
; TODO: fix once exceptions are supported for v1.1
; RUN: llc < %s | FileCheck %s
target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: test
; CHECK:   call foo
; CHECK:   jlt .LBB0_1, .LBB0_2
; CHECK: .LBB0_2:
; CHECK:   ret
; CHECK: .LBB0_1:
; CHECK:   throw
define void @test(i32 %a) personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  invoke void @foo(i32 %a)
          to label %try.cont unwind label %lpad

lpad:
  %0 = landingpad { i8*, i32 } catch i8* null
  call void @llvm.syncvm.throw()
  unreachable

try.cont:
  ret void
}

; CHECK-LABEL: test2
; CHECK:   call foo16
; CHECK:   jlt .LBB1_1, .LBB1_2
; CHECK: .LBB1_2:
; CHECK:   pop #0, r0
; CHECK:   ret
; CHECK: .LBB1_1:
; CHECK:   throw
define void @test2(i32 %a) personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  invoke void @foo16(i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 0, i32 0)
          to label %try.cont unwind label %lpad

lpad:
  %0 = landingpad { i8*, i32 } catch i8* null
  call void @llvm.syncvm.throw()
  unreachable

try.cont:
  ret void
}

; CHECK-LABEL: test3
; CHECK: .LBB2_4:
; CHECK:   pop #0, r0
; CHECK:   j .LBB2_3, .LBB2_3
define void @test3(i32 %a) personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  %cmp = icmp ugt i32 %a, 42
  br i1 %cmp, label %try.cont, label %invokebb

invokebb:
  invoke void @foo16(i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a, i32 %a)
          to label %try.cont unwind label %lpad

lpad:
  %0 = landingpad { i8*, i32 } catch i8* null
  call void @llvm.syncvm.throw()
  unreachable

try.cont:
  ret void
}

; CHECK-LABEL: invoke_sstore
; CHECK: st
; CHECK: ret
define i256 @invoke_sstore() personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  invoke void @llvm.syncvm.sstore(i256 0, i256 0, i256 0)
    to label %return unwind label %lpad
return:
  ret i256 0
lpad:
  %x = landingpad { i8*, i32 } catch i8* null
  ret i256 42
}

declare void @foo(i32)
declare void @foo16(i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)
declare i32 @__gxx_personality_v0(...)
declare void @llvm.syncvm.throw()
declare void @llvm.syncvm.sstore(i256, i256, i256)
