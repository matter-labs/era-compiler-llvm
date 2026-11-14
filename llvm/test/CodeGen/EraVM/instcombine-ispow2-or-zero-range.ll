; RUN: opt -passes=instcombine -S < %s | FileCheck %s

; Check that folding (x == 0 || ctpop(x) == 1) into ctpop(x) u< 2, and
; (x != 0 && ctpop(x) != 1) into ctpop(x) u> 1, drops the range(1, 257)
; attribute of ctpop. The range only holds where x != 0. The folded compare
; is also evaluated for x == 0, so keeping the range would make the result
; poison there instead of true/false.

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i1 @is_power2_or_zero_with_range(i256 %x) {
; CHECK-LABEL: @is_power2_or_zero_with_range(
; CHECK-NEXT:    [[CTPOP:%.*]] = call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 [[X:%.*]])
; CHECK-NEXT:    [[RES:%.*]] = icmp ult i256 [[CTPOP]], 2
; CHECK-NEXT:    ret i1 [[RES]]
;
  %ctpop = call range(i256 1, 257) i256 @llvm.ctpop.i256(i256 %x)
  %cmp = icmp eq i256 %ctpop, 1
  %iszero = icmp eq i256 %x, 0
  %res = select i1 %iszero, i1 true, i1 %cmp
  ret i1 %res
}

define i1 @not_power2_or_zero_with_range(i256 %x) {
; CHECK-LABEL: @not_power2_or_zero_with_range(
; CHECK-NEXT:    [[CTPOP:%.*]] = call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 [[X:%.*]])
; CHECK-NEXT:    [[RES:%.*]] = icmp ugt i256 [[CTPOP]], 1
; CHECK-NEXT:    ret i1 [[RES]]
;
  %ctpop = call range(i256 1, 257) i256 @llvm.ctpop.i256(i256 %x)
  %cmp = icmp ne i256 %ctpop, 1
  %notzero = icmp ne i256 %x, 0
  %res = select i1 %notzero, i1 %cmp, i1 false
  ret i1 %res
}
