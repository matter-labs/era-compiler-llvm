; RUN: opt -passes='default<O3>' -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; Check that ScalarEvolution::isImpliedViaMerge does not treat an addrec
; incoming value (%indvar3 from the previous iteration) as equal to the current
; iteration's %indvar3, which would fold (%indvar2 > 0) to true.
define i256 @test_imply_merge_addrec(i8 %n8, ptr addrspace(1) %p) {
; CHECK-LABEL: @test_imply_merge_addrec(
; CHECK-NOT:   zext i1 true to i256
; CHECK:       [[COND_PEEL4_NOT:%.*]] = icmp ne i8 %n8, 2
; CHECK:       [[EXT:%.*]] = zext i1 [[COND_PEEL4_NOT]] to i256
; CHECK:       store i256 [[EXT]], ptr addrspace(1) %p
;
entry:
  %n = zext i8 %n8 to i256
  br label %for.preheader

for.preheader:
  %indvar2 = phi i256 [ 1, %entry ], [ %indvar3, %for.inc ]
  %indvar3 = phi i256 [ 0, %entry ], [ %inc, %for.inc ]
  %cond1 = icmp eq i256 %indvar3, 0
  br i1 %cond1, label %for.inc, label %for.end

for.end:
  %cmp = icmp sgt i256 %indvar2, 0
  %ext = zext i1 %cmp to i256
  store i256 %ext, ptr addrspace(1) %p
  br label %for.inc

for.inc:
  %phi = phi i256 [ %ext, %for.end ], [ 0, %for.preheader ]
  %inc = add i256 %indvar3, 1
  %cond2 = icmp ult i256 %inc, %n
  br i1 %cond2, label %for.preheader, label %exit

exit:
  ret i256 %phi
}
