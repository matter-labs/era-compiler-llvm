; RUN: opt -passes=loop-versioning -S < %s | FileCheck %s

; Check that SCEVExpander drops poison-generating flags from a GEP that it
; reuses. The runtime alias check needs %a + %n and reuses %a.end, which
; is only used under %c. The inbounds flag is not known to hold for the new
; use, so it must be cleared.

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define void @reuse_gep(ptr addrspace(1) %a, ptr addrspace(1) %b, i256 %n, i1 %c) {
; CHECK-LABEL: @reuse_gep(
; CHECK:       loop.lver.check:
; CHECK-NEXT:    %a.end = getelementptr i8, ptr addrspace(1) %a, i256 %n
; CHECK-NEXT:    %scevgep = getelementptr i8, ptr addrspace(1) %b, i256 %n
; CHECK-NEXT:    %bound0 = icmp ult ptr addrspace(1) %a, %scevgep
; CHECK-NEXT:    %bound1 = icmp ult ptr addrspace(1) %b, %a.end
;
entry:
  %a.end = getelementptr inbounds i8, ptr addrspace(1) %a, i256 %n
  br label %loop

loop:
  %i = phi i256 [ 0, %entry ], [ %i.next, %loop ]
  %pb = getelementptr i8, ptr addrspace(1) %b, i256 %i
  %v = load i8, ptr addrspace(1) %pb, align 1
  %pa = getelementptr i8, ptr addrspace(1) %a, i256 %i
  store i8 %v, ptr addrspace(1) %pa, align 1
  %i.next = add nuw nsw i256 %i, 1
  %ec = icmp eq i256 %i.next, %n
  br i1 %ec, label %exit, label %loop

exit:
  br i1 %c, label %use, label %ret

use:
  store i8 0, ptr addrspace(1) %a.end, align 1
  br label %ret

ret:
  ret void
}
