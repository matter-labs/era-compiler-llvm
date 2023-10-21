; RUN: llc -O3 < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; TODO: CPR-1421 Remove loop index
; The register r2 and r1 are pointers and will be increased per iteration,
; we can use them to judge whether loop should exit, the add instruction used
; to increase loop index r4 can be optimized out.
define void @loop1(i256 addrspace(1)* %dest, i256 addrspace(1)* %src, i256 %size) {
; CHECK:        add     r0, r0, r4
; CHECK-NEXT: .BB0_1:
; CHECK:        ld.1.inc   r2, r5, r2
; CHECK-NEXT:   st.1.inc   r1, r5, r1
; CHECK-NEXT:   add        1, r4, r4
; CHECK-NEXT:   sub!       r4, r3, r5
; CHECK-NEXT:   jump.lt @.BB0_1
; CHECK:        ret

  br label %load-store-loop

load-store-loop:                                  ; preds = %load-store-loop, %0
  %loop-index = phi i256 [ 0, %0 ], [ %4, %load-store-loop ]
  %1 = getelementptr inbounds i256, i256 addrspace(1)* %src, i256 %loop-index
  %2 = load i256, i256 addrspace(1)* %1, align 1
  %3 = getelementptr inbounds i256, i256 addrspace(1)* %dest, i256 %loop-index
  store i256 %2, i256 addrspace(1)* %3, align 1
  %4 = add i256 %loop-index, 1
  %5 = icmp ult i256 %4, %size
  br i1 %5, label %load-store-loop, label %memcpy-split

memcpy-split:                                     ; preds = %load-store-loop
  ret void
}

define void @loop2(i256 addrspace(1)* %dest, i256 addrspace(1)* %src, i256 %size) {
; CHECK:        add     10, r0, r4
; CHECK-NEXT:   add     320, r1, r1
; CHECK-NEXT:   add     320, r2, r2
; CHECK-NEXT: .BB1_1:
; CHECK:        ld.1.inc   r2, r5, r2
; CHECK-NEXT:   st.1.inc   r1, r5, r1
; CHECK-NEXT:   add        1, r4, r4
; CHECK-NEXT:   sub!       r4, r3, r5
; CHECK-NEXT:   jump.lt @.BB1_1
; CHECK:        ret

entry:
  br label %load-store-loop

load-store-loop:                                  ; preds = %load-store-loop, %entry
  %loop-index = phi i256 [ 10, %entry ], [ %3, %load-store-loop ]
  %0 = getelementptr inbounds i256, i256 addrspace(1)* %src, i256 %loop-index
  %1 = load i256, i256 addrspace(1)* %0, align 1
  %2 = getelementptr inbounds i256, i256 addrspace(1)* %dest, i256 %loop-index
  store i256 %1, i256 addrspace(1)* %2, align 1
  %3 = add i256 %loop-index, 1
  %4 = icmp ult i256 %3, %size
  br i1 %4, label %load-store-loop, label %memcpy-split

memcpy-split:                                     ; preds = %load-store-loop
  ret void
}

define void @loop3(i256 addrspace(1)* %dest, i256 addrspace(1)* %src, i256 %size, i256 %pos) {
; CHECK:        shl.s   5, r4, r4
; CHECK-NEXT:   add     r2, r4, r4
; CHECK-NEXT:   add     10, r0, r2
; CHECK-NEXT:   add     32, r4, r4
; CHECK-NEXT: .BB2_1:
; CHECK:        shl.s      5,  r2, r5
; CHECK-NEXT:   add        r1, r5, r5
; CHECK-NEXT:   ld.1.inc   r4, r6, r4
; CHECK-NEXT:   st.1       r5, r6
; CHECK-NEXT:   add        2, r2, r2
; CHECK-NEXT:   sub!       r2, r3, r5
; CHECK-NEXT:   jump.lt @.BB2_1
; CHECK:        ret

entry:
  %src-index-init = add i256 %pos, 1
  br label %load-store-loop

load-store-loop:                                  ; preds = %load-store-loop, %entry
  %loop-index = phi i256 [ 10, %entry ], [ %3, %load-store-loop ]
  %src-index = phi i256 [ %src-index-init, %entry ], [ %src-index-inc, %load-store-loop ]
  %0 = getelementptr inbounds i256, i256 addrspace(1)* %src, i256 %src-index
  %1 = load i256, i256 addrspace(1)* %0, align 1
  %2 = getelementptr inbounds i256, i256 addrspace(1)* %dest, i256 %loop-index
  store i256 %1, i256 addrspace(1)* %2, align 1
  %src-index-inc = add i256 %src-index, 1
  %3 = add i256 %loop-index, 2
  %4 = icmp ult i256 %3, %size
  br i1 %4, label %load-store-loop, label %memcpy-split

memcpy-split:                                     ; preds = %load-store-loop
  ret void
}

define void @loop4([10 x i256] addrspace(1)* %dest, [10 x i256] addrspace(1)* %src, i256 %size) {
; CHECK:        add     r0, r0, r4
; CHECK-NEXT: .BB3_1:
; CHECK:        ld.1.inc   r2, r5, r2
; CHECK-NEXT:   st.1.inc   r1, r5, r1
; CHECK-NEXT:   add        1, r4, r4
; CHECK-NEXT:   sub!       r4, r3, r5
; CHECK-NEXT:   jump.lt @.BB3_1
; CHECK:        ret
  br label %load-store-loop

load-store-loop:                                  ; preds = %load-store-loop, %0
  %loop-index = phi i256 [ 0, %0 ], [ %4, %load-store-loop ]
  %1 = getelementptr [10 x i256], [10 x i256] addrspace(1)* %src, i256 0, i256 %loop-index
  %2 = load i256, i256 addrspace(1)* %1, align 1
  %3 = getelementptr [10 x i256], [10 x i256] addrspace(1)* %dest, i256 0, i256 %loop-index
  store i256 %2, i256 addrspace(1)* %3, align 1
  %4 = add i256 %loop-index, 1
  %5 = icmp ult i256 %4, %size
  br i1 %5, label %load-store-loop, label %memcpy-split

memcpy-split:                                     ; preds = %load-store-loop
  ret void
}

define void @loop5([10 x i256] addrspace(1)* %dest, [10 x i256] addrspace(1)* %src, i256 %size) {
; CHECK:        add     320, r1, r1
; CHECK-NEXT:   add     320, r2, r2
; CHECK-NEXT:   add     r0, r0, r4
; CHECK-NEXT: .BB4_1:
; CHECK:        ld.1.inc   r2, r5, r2
; CHECK-NEXT:   st.1.inc   r1, r5, r1
; CHECK-NEXT:   add        1, r4, r4
; CHECK-NEXT:   sub!       r4, r3, r5
; CHECK-NEXT:   jump.lt @.BB4_1
; CHECK:        ret
  br label %load-store-loop

load-store-loop:                                  ; preds = %load-store-loop, %0
  %loop-index = phi i256 [ 0, %0 ], [ %4, %load-store-loop ]
  %1 = getelementptr [10 x i256], [10 x i256] addrspace(1)* %src, i256 1, i256 %loop-index
  %2 = load i256, i256 addrspace(1)* %1, align 1
  %3 = getelementptr [10 x i256], [10 x i256] addrspace(1)* %dest, i256 1, i256 %loop-index
  store i256 %2, i256 addrspace(1)* %3, align 1
  %4 = add i256 %loop-index, 1
  %5 = icmp ult i256 %4, %size
  br i1 %5, label %load-store-loop, label %memcpy-split

memcpy-split:                                     ; preds = %load-store-loop
  ret void
}
