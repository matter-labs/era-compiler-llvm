; RUN: opt -passes='default<O3>' -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

; Check that SCEV does not rewrite %p to a shifted expression containing
; 173 /u {0,+,1}, which would give %p an empty range and cause IndVars to
; fold (%p < %x) to true (returning 1 unconditionally).
define i256 @test_shifted_urem_ub(i256 %x) {
; CHECK-LABEL: @test_shifted_urem_ub(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP:%.*]] = icmp ugt i256 [[X:%.*]], 83
; CHECK-NEXT:    [[RES:%.*]] = zext i1 [[CMP]] to i256
; CHECK-NEXT:    ret i256 [[RES]]
;
entry:
  br label %for.cond

for.cond:
  %i = phi i256 [ 1, %entry ], [ %i.next, %for.body ]
  %p = phi i256 [ 83, %entry ], [ %p.next, %for.body ]
  %res = phi i256 [ 0, %entry ], [ %res.next, %for.body ]
  %mul = mul nuw nsw i256 %i, %i
  %cond = icmp ult i256 %mul, 4
  br i1 %cond, label %for.body, label %exit

for.body:
  %cmp = icmp ult i256 %p, %x
  %res.next = select i1 %cmp, i256 1, i256 %res
  %rem = urem i256 173, %i
  %sub = sub i256 0, %rem
  %p.next = and i256 %sub, 255
  %i.next = add nuw nsw i256 %i, 1
  br label %for.cond

exit:
  ret i256 %res
}
