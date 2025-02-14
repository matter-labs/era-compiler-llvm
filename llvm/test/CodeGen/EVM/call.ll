; RUN: llc --evm-keep-registers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

declare i256 @foo(i256)
declare void @foo2(i256)

define i256 @call(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: @call
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ADD [[TMP1:\$[0-9]+]], [[IN1]], [[IN2]]
; CHECK: FCALL 1 [[RES1:\$[0-9]+]], @foo, [[TMP1]]

  %sum = add i256 %a, %b
  %res = call i256 @foo(i256 %sum)
  ret i256 %res
}

define void @call2(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: @call2
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ADD [[TMP1:\$[0-9]+]], [[IN1]], [[IN2]]
; CHECK: FCALL 0 @foo2, [[TMP1]]

  %sum = add i256 %a, %b
  call void @foo2(i256 %sum)
  ret void
}

define void @call3_indir(void (i256)* %callee) nounwind {
; CHECK-LABEL: @call3_indir
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[C1:\$[0-9]+]], 10
; CHECK: FCALL 0 [[IN1]], [[C1]]

  call void %callee(i256 10)
  ret void
}

define i256 @call4_indir(i256 (i256)* %callee) nounwind {
; CHECK-LABEL: @call4_indir
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: CONST_I256 [[C1:\$[0-9]+]], 10
; CHECK: FCALL 1 [[RES1:\$[0-9]+]], [[IN1]], [[C1]]

  %res = call i256 %callee(i256 10)
  ret i256 %res
}
