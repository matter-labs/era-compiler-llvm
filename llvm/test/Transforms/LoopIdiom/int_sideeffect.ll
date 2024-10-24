; RUN: opt -S < %s -passes=loop-idiom | FileCheck %s
; XFAIL: target=eravm{{.*}}, target=evm{{.*}}

declare void @llvm.sideeffect()

; Loop idiom recognition across a @llvm.sideeffect.

; CHECK-LABEL: zero
; CHECK: llvm.memset
define void @zero(ptr %p, i64 %n) nounwind {
bb7.lr.ph:
  br label %bb7

bb7:
  %i.02 = phi i64 [ 0, %bb7.lr.ph ], [ %tmp13, %bb7 ]
  %tmp10 = getelementptr inbounds float, ptr %p, i64 %i.02
  store float 0.000000e+00, ptr %tmp10, align 4
  %tmp13 = add i64 %i.02, 1
  %tmp6 = icmp ult i64 %tmp13, %n
  br i1 %tmp6, label %bb7, label %bb14

bb14:
  ret void
}
