; RUN: llc -O3 < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define void @test(ptr addrspace(1) %0, ptr addrspace(1) %1, i256 %2) {
; CHECK-LABEL: test:
; CHECK:       sub! r3, r0, r4
; CHECK-NEXT:  jump.eq @.BB0_3
; CHECK:       .BB0_2:
; CHECK:       add r1, r3, r4
; CHECK-NEXT:  add @CPI0_0[0], r4, r4
; CHECK-NEXT:  add r2, r3, r5
; CHECK-NEXT:  add @CPI0_0[0], r5, r5
; CHECK-NEXT:  ld.1 r5, r5
; CHECK-NEXT:  st.1 r4, r5
; CHECK-NEXT:  sub.s! 32, r3, r3
; CHECK-NEXT:  jump.ne @.BB0_2
; CHECK:       .BB0_3:
; CHECK-NEXT:  ret
  %4 = icmp eq i256 %2, 0
  br i1 %4, label %12, label %5

5:
  %6 = phi i256 [ %7, %5 ], [ %2, %3 ]
  %7 = add i256 %6, -32
  %8 = getelementptr inbounds i8, ptr addrspace(1) %1, i256 %7
  %9 = load i256, ptr addrspace(1) %8, align 4
  %10 = getelementptr inbounds i8, ptr addrspace(1) %0, i256 %7
  store i256 %9, ptr addrspace(1) %10, align 4
  %11 = icmp eq i256 %7, 0
  br i1 %11, label %12, label %5

12:
  ret void
}

; During lowering of a GEP with multiple indices into ptrtoint+arithmetics+inttoptr form,
; SeparateConstOffsetFromGEP pass is emitting wrong negative offset index. Instead of -32,
; it is emitting uint64_t representation of -32 (18446744073709551584), so that number ends
; up in the CP.
; CHECK: CPI0_0:
; CHECK: .cell 18446744073709551584
