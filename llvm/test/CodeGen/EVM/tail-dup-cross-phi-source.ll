; RUN: llc -mtriple=evm -O3 -stop-after=finalize-isel < %s | llc -mtriple=evm -x mir -run-pass=early-tailduplication -o - | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

; Regression test for https://github.com/llvm/llvm-project/pull/194832
; (commit 5a155740c8e6) on EVM i256: when TailBB (%header) has two PHIs where
; %p1 references %p0 along a non-duplicated layout predecessor
; (%pred_fallthrough), TailDuplicator::processPHI with Remove=false must
; preserve the previous iteration's %p0 for %p1 rather than rewriting it to
; %next (%p0 + 1).

@addr = global ptr blockaddress(@test_cross_phi, %header)

define i256 @test_cross_phi(i256 %init0) {
; CHECK-LABEL: name: test_cross_phi
; CHECK:       bb.2.pred_fallthrough:
; CHECK:         [[PREV:%[0-9]+]]:gpr = PHI [[P0:%[0-9]+]], %bb.3, {{%[0-9]+}}, %bb.0
; CHECK-NEXT:    [[ONE:%[0-9]+]]:gpr = CONST_I256 i256 1, implicit-def dead $arguments
; CHECK-NEXT:    [[NEXT:%[0-9]+]]:gpr = ADD [[PREV]], killed [[ONE]], implicit-def dead $arguments
; CHECK-NEXT:    [[COPY_PREV:%[0-9]+]]:gpr = COPY [[PREV]]
; CHECK:       bb.3.header (ir-block-address-taken %ir-block.header):
; CHECK:         [[P1:%[0-9]+]]:gpr = PHI [[COPY_PREV]], %bb.2
; CHECK-NEXT:    [[P0]]:gpr = PHI [[NEXT]], %bb.2
entry:
  br label %header

exit:
  ret i256 %p1

pred_fallthrough:
  %next = add i256 %p0, 1
  br label %header

header:
  %p1 = phi i256 [ 0, %entry ], [ %p0, %pred_fallthrough ]
  %p0 = phi i256 [ %init0, %entry ], [ %next, %pred_fallthrough ]
  %done = icmp eq i256 %p0, 0
  br i1 %done, label %exit, label %pred_fallthrough
}
