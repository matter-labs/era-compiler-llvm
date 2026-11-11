; RUN: opt -passes=instcombine -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; Calling a void function as if it returned a value is undefined behavior.
; InstCombine must not turn such a call into a void call and replace its
; result with poison. Converting the call is fine if its result is unused.

define void @callee() {
entry:
  ret void
}

define i256 @use_result() {
; CHECK-LABEL: @use_result(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[R:%.*]] = tail call i256 @callee()
; CHECK-NEXT:    ret i256 [[R]]
entry:
  %r = tail call i256 @callee()
  ret i256 %r
}

define i256 @use_result_add(i256 %x) {
; CHECK-LABEL: @use_result_add(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[R:%.*]] = call i256 @callee()
; CHECK-NEXT:    [[S:%.*]] = add i256 [[R]], [[X:%.*]]
; CHECK-NEXT:    ret i256 [[S]]
entry:
  %r = call i256 @callee()
  %s = add i256 %r, %x
  ret i256 %s
}

define void @unused_result() {
; CHECK-LABEL: @unused_result(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    call void @callee()
; CHECK-NEXT:    ret void
entry:
  %r = call i256 @callee()
  ret void
}
