; RUN: opt -passes=mergebb -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define void @test() {
; CHECK-LABEL: @test(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label [[RETURN_LOOPEXIT:%.*]]
; CHECK:       return.loopexit:
; CHECK-NEXT:    [[VAR_Y7_US:%.*]] = alloca i256, align 32
; CHECK-NEXT:    unreachable
;
entry:
  br label %return.loopexit

return.loopexit:                                  ; preds = %entry
  %var_y7.us = alloca i256, align 32
  store i256 1, ptr null, align 4294967296
  br label %return

return:                                           ; preds = %for_join499, %if_join349, %return.loopexit
  %var_y7.lcssa = phi ptr [ null, %if_join349 ], [ null, %return.loopexit ], [ %var_y7, %for_join499 ]
  ret void

if_join349:                                       ; No predecessors!
  %var_y7 = alloca i256, align 32
  store i256 1, ptr null, align 4294967296
  br label %return

for_join499:                                      ; No predecessors!
  br label %return
}
