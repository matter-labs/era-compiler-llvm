; RUN: llc -mtriple=eravm -O3 < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

; Regression test for https://github.com/llvm/llvm-project/pull/194832
; (commit 5a155740c8e6): when EarlyTailDuplicate duplicates TailBB (%header)
; into some predecessors but leaves a non-duplicated layout predecessor
; (%pred_fallthrough) falling through into TailBB, TailDuplicator::processPHI
; must update the PHI operand in TailBB to NewDef rather than registering
; DefReg -> NewDef in MachineSSAUpdater for PredBB. Otherwise, a second PHI
; (%p1) in TailBB whose incoming value from %pred_fallthrough is %p0 gets
; rewritten to %next (%p0 + 1) instead of the previous value of %p0.

define i256 @test_cross_phi(i256 %init0, ptr addrspace(1) %ptr) {
; CHECK-LABEL: test_cross_phi:
; CHECK:       .BB0_3: ; %pred_fallthrough
; CHECK-NEXT:    add 1, r1, r3
; CHECK-NEXT:  .BB0_4: ; %header
; CHECK-NEXT:    add r1, r0, r4
; CHECK-NEXT:    add r3, r0, r1
; CHECK-NEXT:    ldm.h r2, r5
; CHECK-NEXT:    add r4, r0, r3
; CHECK-NEXT:    jump code[@JTI0_0+r5]
entry:
  br label %header

pred_cond:
  %v = load volatile i256, ptr addrspace(1) %ptr, align 32
  %c = icmp eq i256 %p0, %v
  br i1 %c, label %header.backedge, label %pred_other

pred_other:
  br label %header.backedge

pred_fallthrough:
  %next = add i256 %p0, 1
  br label %header.backedge

header:
  %p1 = phi i256 [ 0, %entry ], [ %p0, %header.backedge ]
  %p0 = phi i256 [ %init0, %entry ], [ %p0.be, %header.backedge ]
  %choice = load volatile i256, ptr addrspace(1) %ptr, align 32
  switch i256 %choice, label %unreach [
    i256 0, label %pred_dup
    i256 1, label %pred_cond
    i256 2, label %pred_fallthrough
    i256 3, label %exit
  ]

pred_dup:
  %val = load volatile i256, ptr addrspace(1) %ptr, align 32
  br label %header.backedge

header.backedge:
  %p0.be = phi i256 [ %val, %pred_dup ], [ %next, %pred_fallthrough ], [ 0, %pred_other ], [ %p1, %pred_cond ]
  br label %header

exit:
  ret i256 %p1

unreach:
  unreachable
}
