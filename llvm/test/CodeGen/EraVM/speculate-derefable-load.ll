; RUN: opt -passes=simplifycfg -S < %s | FileCheck %s

; Check that SimplifyCFG doesn't speculate a load whose pointer is only known
; to be dereferenceable through the !dereferenceable metadata of another
; conditional load. The metadata holds only on the path where %c is true, so
; neither load may be hoisted to the entry block.

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @deref_no_hoist(i1 %c, ptr addrspace(1) align 32 dereferenceable(32) %p1) {
; CHECK-LABEL: @deref_no_hoist(
; CHECK:       entry:
; CHECK-NEXT:    br i1 %c, label %if, label %exit
; CHECK:       if:
; CHECK-NEXT:    %p2 = load ptr addrspace(1), ptr addrspace(1) %p1, align 32, !dereferenceable !0, !align !0
; CHECK-NEXT:    %v = load i256, ptr addrspace(1) %p2, align 32
; CHECK-NEXT:    br label %exit
; CHECK:       exit:
; CHECK-NEXT:    %res = phi i256 [ %v, %if ], [ 0, %entry ]
; CHECK-NEXT:    ret i256 %res
;
entry:
  br i1 %c, label %if, label %exit

if:
  %p2 = load ptr addrspace(1), ptr addrspace(1) %p1, align 32, !dereferenceable !0, !align !0
  %v = load i256, ptr addrspace(1) %p2, align 32
  br label %exit

exit:
  %res = phi i256 [ %v, %if ], [ 0, %entry ]
  ret i256 %res
}

!0 = !{i64 32}
