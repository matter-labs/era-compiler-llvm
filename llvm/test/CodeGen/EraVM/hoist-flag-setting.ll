; RUN: llc -O3 < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test_hoist_1(i256 %0) {
; CHECK-LABEL: test_hoist_1
; CHECK:      .BB0_2:
; CHECK:        and! 2, r1, r0
; CHECK-NEXT:   shr.s 1, r1, r1
; CHECK-NEXT:   add 1, r2, r2
; CHECK-NEXT:   jump.ne @.BB0_2

  %2 = and i256 %0, 1
  %3 = icmp eq i256 %2, 0
  br i1 %3, label %11, label %4

4:
  %5 = phi i256 [ %7, %4 ], [ 0, %1 ]
  %6 = phi i256 [ %8, %4 ], [ %0, %1 ]
  %7 = add nuw nsw i256 %5, 1
  %8 = lshr i256 %6, 1
  %9 = and i256 %6, 2
  %10 = icmp eq i256 %9, 0
  br i1 %10, label %11, label %4

11:
  %12 = phi i256 [ 0, %1 ], [ %7, %4 ]
  ret i256 %12
}

define i256 @test_hoist_2(i256 %0, i256 %1, i256 addrspace(1)* %2, i256 addrspace(1)* %3) {
; CHECK-LABEL: test_hoist_2
; CHECK:      .BB1_5:
; CHECK:        and! 2, r1, r0
; CHECK-NEXT:   shr.s 1, r1, r1
; CHECK-NEXT:   add 1, r2, r2
; CHECK-NEXT:   add 1, r5, r5
; CHECK-NEXT:   jump.eq @.BB1_6

  %5 = and i256 %0, 1
  %6 = icmp eq i256 %5, 0
  br i1 %6, label %22, label %7

7:
  %8 = phi i256 [ %17, %16 ], [ 0, %4 ]
  %9 = phi i256 [ %19, %16 ], [ %0, %4 ]
  %10 = phi i256 [ %18, %16 ], [ %1, %4 ]
  %11 = icmp ugt i256 %10, 100
  br i1 %11, label %12, label %14

12:
  %13 = getelementptr inbounds i256, i256 addrspace(1)* %2, i256 %8
  store i256 5, i256 addrspace(1)* %13, align 8
  br label %16

14:
  %15 = getelementptr inbounds i256, i256 addrspace(1)* %3, i256 %10
  store i256 10, i256 addrspace(1)* %15, align 8
  br label %16

16:
  %17 = add i256 %8, 1
  %18 = add i256 %10, 1
  %19 = lshr i256 %9, 1
  %20 = and i256 %9, 2
  %21 = icmp eq i256 %20, 0
  br i1 %21, label %22, label %7

22:
  %23 = phi i256 [ 0, %4 ], [ %17, %16 ]
  ret i256 %23
}

define i256 @test_hoist_const(i256 %0) {
; CHECK-LABEL: test_hoist_const
; CHECK:      .BB2_2:
; CHECK:        and! @CPI2_0[0], r1, r0
; CHECK-NEXT:   mul @CPI2_0[0], r1, r1, r0
; CHECK-NEXT:   add 1, r2, r2
; CHECK-NEXT:   jump.ne @.BB2_2

  %2 = and i256 %0, 1
  %3 = icmp eq i256 %2, 0
  br i1 %3, label %11, label %4

4:
  %5 = phi i256 [ %7, %4 ], [ 0, %1 ]
  %6 = phi i256 [ %8, %4 ], [ %0, %1 ]
  %7 = add nuw nsw i256 %5, 1
  %8 = mul i256 %6, 100000
  %9 = and i256 %6, 100000
  %10 = icmp eq i256 %9, 0
  br i1 %10, label %11, label %4

11:
  %12 = phi i256 [ 0, %1 ], [ %7, %4 ]
  ret i256 %12
}

define void @test_hoist_inc(i256 addrspace(1)* %dst, i256 %end) {
; CHECK-LABEL: test_hoist_inc
; CHECK:      .BB3_1:
; CHECK:        sub! r1, r2, r0
; CHECK-NEXT:   st.1.inc r1, r3, r1
; CHECK-NEXT:   jump.ne @.BB3_1

entry:
  %gep1 = getelementptr inbounds i256, i256 addrspace(1)* %dst, i256 10
  br label %loop

loop:
  %phi = phi i256 addrspace(1)* [ %gep1, %entry ], [ %gep2, %loop ]
  %gep2 = getelementptr inbounds i256, i256 addrspace(1)* %phi, i256 1
  store i256 1, i256 addrspace(1)* %phi, align 1
  %ptrtoint = ptrtoint i256 addrspace(1)* %phi to i256
  %cmp = icmp ne i256 %ptrtoint, %end
  br i1 %cmp, label %loop, label %ret

ret:
  ret void
}
