; RUN: llc < %s | FileCheck %s
target datalayout = "e-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: test
; CHECK:   call foo
; CHECK:   jlt .LBB0_1, .LBB0_2
; CHECK: .LBB0_2:
; CHECK:   ret
; CHECK: .LBB0_1:
; CHECK:   throw
define void @test(i32 %a, i32 %b) personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
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

declare void @foo(i32)
declare i32 @__gxx_personality_v0(...)
declare void @llvm.syncvm.throw()
