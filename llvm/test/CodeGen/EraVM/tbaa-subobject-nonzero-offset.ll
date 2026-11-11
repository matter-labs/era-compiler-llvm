; RUN: opt -O3 -S < %s | FileCheck %s

; Check that TBAA reports MayAlias for an access to a whole aggregate and an
; access to one of its members at a non-zero offset. %a may be equal to
; %p + 32, so the load must not be folded to the first stored value.

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; struct A { uint256 i; uint256 j; };
define i512 @test(ptr addrspace(1) %p, ptr addrspace(1) %a) {
; CHECK-LABEL: @test(
; CHECK:         store i512 5, ptr addrspace(1) %p
; CHECK:         store i256 7, ptr addrspace(1) %a
; CHECK:         [[RET:%.*]] = load i512, ptr addrspace(1) %p
; CHECK:         ret i512 [[RET]]
;
  store i512 5, ptr addrspace(1) %p, align 32, !tbaa !4  ; TAG_A
  store i256 7, ptr addrspace(1) %a, align 32, !tbaa !5  ; TAG_A_j
  %v = load i512, ptr addrspace(1) %p, align 32, !tbaa !4  ; TAG_A
  ret i512 %v
}

!0 = !{!"root"}
!1 = !{!0, i64 1, !"char"}
!2 = !{!1, i64 32, !"uint256"}
!3 = !{!1, i64 64, !"A", !2, i64 0, i64 32, !2, i64 32, i64 32}
!4 = !{!3, !3, i64 0, i64 64}  ; TAG_A
!5 = !{!3, !2, i64 32, i64 32} ; TAG_A_j
