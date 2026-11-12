; RUN: opt -passes=instcombine -S < %s | FileCheck %s

; Check that InstCombine doesn't move a range(i256 ...) attribute onto a
; <2 x i128> argument or return value when it rewrites a call whose type
; doesn't match the callee. Before the fix opt failed with "Range bit width
; must match type bit width!". Now the range attribute is treated as
; incompatible with the new type, and the calls are left unchanged.

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define internal void @callee_arg(<2 x i128> %v) {
  ret void
}

define void @range_on_cast_arg(i256 %x) {
; CHECK-LABEL: @range_on_cast_arg(
; CHECK-NEXT:    call void @callee_arg(i256 range(i256 0, 10) [[X:%.*]])
; CHECK-NEXT:    ret void
;
  call void @callee_arg(i256 range(i256 0, 10) %x)
  ret void
}

define internal <2 x i128> @callee_ret() {
  ret <2 x i128> zeroinitializer
}

define i256 @range_on_cast_ret() {
; CHECK-LABEL: @range_on_cast_ret(
; CHECK-NEXT:    [[R:%.*]] = call range(i256 0, 10) i256 @callee_ret()
; CHECK-NEXT:    ret i256 [[R]]
;
  %r = call range(i256 0, 10) i256 @callee_ret()
  ret i256 %r
}
