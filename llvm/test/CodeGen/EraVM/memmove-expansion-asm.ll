; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py UTC_ARGS: --version 2
; RUN: llc -O3 --cgp-verify-bfi-updates=false < %s | FileCheck %s
; Verification of BFI updates is disabled because of https://github.com/llvm/llvm-project/issues/64197

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @llvm.memmove.p1.p1.i256(ptr addrspace(1), ptr addrspace(1), i256, i1 immarg)

define i256 @expand_unknown(ptr addrspace(1) %dst, ptr addrspace(1) %src, i256 %size) {
; CHECK-LABEL: expand_unknown:
; CHECK:       ; %bb.0: ; %entry
; CHECK-NEXT:    and code[@CPI0_0], r3, r4
; CHECK-NEXT:    and 31, r3, r3
; CHECK-NEXT:    sub! r2, r1, r0
; CHECK-NEXT:    jump.ge @.BB0_5
; CHECK-NEXT:  ; %bb.1: ; %copy-backwards
; CHECK-NEXT:    sub! r4, r0, r0
; CHECK-NEXT:    jump.eq @.BB0_4
; CHECK-NEXT:  ; %bb.2: ; %copy-backwards-loop-preheader
; CHECK-NEXT:    add r2, r3, r6
; CHECK-NEXT:    add r1, r3, r5
; CHECK-NEXT:    sub.s 32, r5, r5
; CHECK-NEXT:    sub.s 32, r6, r6
; CHECK-NEXT:  .BB0_3: ; %copy-backwards-loop
; CHECK-NEXT:    ; =>This Inner Loop Header: Depth=1
; CHECK-NEXT:    add r5, r4, r7
; CHECK-NEXT:    add r6, r4, r8
; CHECK-NEXT:    ldm.h r8, r8
; CHECK-NEXT:    stm.h r7, r8
; CHECK-NEXT:    sub.s! 32, r4, r4
; CHECK-NEXT:    jump.ne @.BB0_3
; CHECK-NEXT:  .BB0_4: ; %copy-backwards-residual-cond
; CHECK-NEXT:    sub! r3, r0, r0
; CHECK-NEXT:    jump.ne @.BB0_10
; CHECK-NEXT:  .BB0_11: ; %memmove-done
; CHECK-NEXT:    add r0, r0, r1
; CHECK-NEXT:    ret
; CHECK-NEXT:  .BB0_5: ; %copy-forward
; CHECK-NEXT:    add r1, r4, r5
; CHECK-NEXT:    sub! r4, r0, r0
; CHECK-NEXT:    jump.eq @.BB0_8
; CHECK-NEXT:  ; %bb.6: ; %copy-forward-loop-preheader
; CHECK-NEXT:    add r2, r0, r6
; CHECK-NEXT:  .BB0_7: ; %copy-forward-loop
; CHECK-NEXT:    ; =>This Inner Loop Header: Depth=1
; CHECK-NEXT:    ldmi.h r6, r7, r6
; CHECK-NEXT:    stmi.h r1, r7, r1
; CHECK-NEXT:    sub! r1, r5, r0
; CHECK-NEXT:    jump.ne @.BB0_7
; CHECK-NEXT:  .BB0_8: ; %copy-forward-residual-cond
; CHECK-NEXT:    sub! r3, r0, r0
; CHECK-NEXT:    jump.eq @.BB0_11
; CHECK-NEXT:  ; %bb.9: ; %copy-forward-residual
; CHECK-NEXT:    add r2, r4, r2
; CHECK-NEXT:    add r5, r0, r1
; CHECK-NEXT:  .BB0_10: ; %memmove-residual
; CHECK-NEXT:    shl.s 3, r3, r3
; CHECK-NEXT:    ldm.h r1, r4
; CHECK-NEXT:    shl r4, r3, r4
; CHECK-NEXT:    shr r4, r3, r4
; CHECK-NEXT:    ldm.h r2, r2
; CHECK-NEXT:    sub 256, r3, r3
; CHECK-NEXT:    shr r2, r3, r2
; CHECK-NEXT:    shl r2, r3, r2
; CHECK-NEXT:    or r2, r4, r2
; CHECK-NEXT:    stm.h r1, r2
; CHECK-NEXT:    add r0, r0, r1
; CHECK-NEXT:    ret
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) %dst, ptr addrspace(1) %src, i256 %size, i1 false)
  ret i256 0
}

define i256 @expand_known_backward() {
; CHECK-LABEL: expand_known_backward:
; CHECK:       ; %bb.0: ; %entry
; CHECK-NEXT:    add 64, r0, r1
; CHECK-NEXT:  .BB1_1: ; %copy-backwards-loop
; CHECK-NEXT:    ; =>This Inner Loop Header: Depth=1
; CHECK-NEXT:    add 9, r1, r2
; CHECK-NEXT:    ldm.h r2, r2
; CHECK-NEXT:    add 99, r1, r3
; CHECK-NEXT:    stm.h r3, r2
; CHECK-NEXT:    sub.s! 32, r1, r1
; CHECK-NEXT:    jump.ne @.BB1_1
; CHECK-NEXT:  ; %bb.2: ; %copy-backwards-residual-cond
; CHECK-NEXT:    ldm.h 10, r2
; CHECK-NEXT:    and code[@CPI1_0], r2, r1
; CHECK-NEXT:    ldm.h 100, r2
; CHECK-NEXT:    and 255, r2, r2
; CHECK-NEXT:    or r1, r2, r1
; CHECK-NEXT:    stm.h 100, r1
; CHECK-NEXT:    add r0, r0, r1
; CHECK-NEXT:    ret
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), i256 95, i1 false)
  ret i256 0
}

define i256 @expand_known_loop_iter1(ptr addrspace(1) %dst, ptr addrspace(1) %src) {
; CHECK-LABEL: expand_known_loop_iter1:
; CHECK:       ; %bb.0: ; %entry
; CHECK-NEXT:    sub! r2, r1, r0
; CHECK-NEXT:    jump.ge @.BB2_2
; CHECK-NEXT:  ; %bb.1: ; %copy-backwards
; CHECK-NEXT:    add 10, r1, r3
; CHECK-NEXT:    add 10, r2, r4
; CHECK-NEXT:    ldm.h r4, r4
; CHECK-NEXT:    stm.h r3, r4
; CHECK-NEXT:    jump @.BB2_3
; CHECK-NEXT:  .BB2_2: ; %copy-forward
; CHECK-NEXT:    ldmi.h r2, r3, r2
; CHECK-NEXT:    stmi.h r1, r3, r1
; CHECK-NEXT:  .BB2_3: ; %memmove-residual
; CHECK-NEXT:    ldm.h r1, r3
; CHECK-NEXT:    and code[@CPI2_0], r3, r3
; CHECK-NEXT:    ldm.h r2, r2
; CHECK-NEXT:    and code[@CPI2_1], r2, r2
; CHECK-NEXT:    or r2, r3, r2
; CHECK-NEXT:    stm.h r1, r2
; CHECK-NEXT:    add r0, r0, r1
; CHECK-NEXT:    ret
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) %dst, ptr addrspace(1) %src, i256 42, i1 false)
  ret i256 0
}

define i256 @expand_known_loop_iter2(ptr addrspace(1) %dst, ptr addrspace(1) %src) {
; CHECK-LABEL: expand_known_loop_iter2:
; CHECK:       ; %bb.0: ; %entry
; CHECK-NEXT:    sub! r2, r1, r0
; CHECK-NEXT:    jump.ge @.BB3_4
; CHECK-NEXT:  ; %bb.1: ; %copy-backwards
; CHECK-NEXT:    add 64, r0, r3
; CHECK-NEXT:    sub.s 12, r1, r4
; CHECK-NEXT:    sub.s 12, r2, r5
; CHECK-NEXT:  .BB3_2: ; %copy-backwards-loop
; CHECK-NEXT:    ; =>This Inner Loop Header: Depth=1
; CHECK-NEXT:    add r4, r3, r6
; CHECK-NEXT:    add r5, r3, r7
; CHECK-NEXT:    ldm.h r7, r7
; CHECK-NEXT:    stm.h r6, r7
; CHECK-NEXT:    sub.s! 32, r3, r3
; CHECK-NEXT:    jump.ne @.BB3_2
; CHECK-NEXT:  ; %bb.3:
; CHECK-NEXT:    add r1, r0, r3
; CHECK-NEXT:    jump @.BB3_7
; CHECK-NEXT:  .BB3_4: ; %copy-forward
; CHECK-NEXT:    add 64, r1, r3
; CHECK-NEXT:    add r2, r0, r4
; CHECK-NEXT:  .BB3_5: ; %copy-forward-loop
; CHECK-NEXT:    ; =>This Inner Loop Header: Depth=1
; CHECK-NEXT:    ldmi.h r4, r5, r4
; CHECK-NEXT:    stmi.h r1, r5, r1
; CHECK-NEXT:    sub! r1, r3, r0
; CHECK-NEXT:    jump.ne @.BB3_5
; CHECK-NEXT:  ; %bb.6: ; %copy-forward-residual-cond
; CHECK-NEXT:    add 64, r2, r2
; CHECK-NEXT:  .BB3_7: ; %memmove-residual
; CHECK-NEXT:    ldm.h r3, r1
; CHECK-NEXT:    and code[@CPI3_0], r1, r1
; CHECK-NEXT:    ldm.h r2, r2
; CHECK-NEXT:    and code[@CPI3_1], r2, r2
; CHECK-NEXT:    or r2, r1, r1
; CHECK-NEXT:    stm.h r3, r1
; CHECK-NEXT:    add r0, r0, r1
; CHECK-NEXT:    ret
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) %dst, ptr addrspace(1) %src, i256 84, i1 false)
  ret i256 0
}
