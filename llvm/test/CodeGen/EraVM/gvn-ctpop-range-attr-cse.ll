; RUN: opt -passes=gvn -S < %s | FileCheck %s --check-prefix=GVN
; RUN: opt -passes='default<O3>' -S < %s | FileCheck %s --check-prefix=O3

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; GVN kept the attributes of the dominating call when it replaced another call
; to the same function with the same arguments. A range() attribute that holds
; only under a guard then applied to a call that is not guarded.
;
; The Solidity popcount loop `while (y != 0) { y &= y - 1; c++; }` becomes
; llvm.ctpop. Under `x < 256`, InstCombine gives the ctpop range(0, 9).
; SimplifyCFG speculates it out of the guard, and after h is inlined, GVN
; replaced the later, unguarded ctpop(x) (range(0, 257)) with it. `d < 9`
; then folded to true, so f(511, 1) returned 1 instead of 0.
;
; NewGVN (off by default, -eravm-enable-new-gvn-passes) widens the range of the
; kept call as well, but it still folds `d < 9` to true; upstream does the same.

; The IR right before GVN in the caller: the hoisted ctpop has the narrow
; range, the second one has the full range.
; GVN-LABEL: define i256 @gvn_ctpop(
; GVN:         %c = call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 %x)
; GVN-NOT:     call {{.*}}@llvm.ctpop
; GVN:       count:
; GVN-NEXT:    %d.lt9 = icmp ult i256 %c, 9
; GVN-NEXT:    %r = zext i1 %d.lt9 to i256
; GVN:         %ret = phi i256 [ 7, %entry ], [ 5, %join ], [ %r, %count ]
define i256 @gvn_ctpop(i256 %x, i256 %w) {
entry:
  %lt = icmp ult i256 %x, 256
  %c = call range(i256 0, 9) i256 @llvm.ctpop.i256(i256 %x)
  %c.lt3 = icmp ult i256 %c, 3
  %small = select i1 %lt, i1 %c.lt3, i1 false
  br i1 %small, label %exit, label %join

join:
  %wnz = icmp ne i256 %w, 0
  br i1 %wnz, label %count, label %exit

count:
  %d = call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 %x)
  %d.lt9 = icmp ult i256 %d, 9
  %r = zext i1 %d.lt9 to i256
  br label %exit

exit:
  %ret = phi i256 [ 7, %entry ], [ 5, %join ], [ %r, %count ]
  ret i256 %ret
}

; The same code before the pipeline, as zksolc emits it from
;
;   function h(uint256 x, uint256 w) internal pure returns (uint256) {
;     unchecked {
;       uint256 r = 5;
;       if (x < 256) {
;         uint256 c; uint256 y = x;
;         while (y != 0) { y &= y - 1; c++; }
;         if (c < 3) return 7;
;       }
;       if (w != 0) {
;         uint256 d; uint256 z = x;
;         while (z != 0) { z &= z - 1; d++; }
;         r = d < 9 ? 1 : 0;
;       }
;       return r;
;     }
;   }
;
; h has two callers, so it is simplified on its own first (the ctpops get
; their ranges there) and is inlined into f and k afterwards.
define private i256 @h(i256 %x, i256 %w) {
entry:
  %lt = icmp ult i256 %x, 256
  br i1 %lt, label %loop1, label %join

loop1:
  %y = phi i256 [ %x, %entry ], [ %y.next, %body1 ]
  %c = phi i256 [ 0, %entry ], [ %c.next, %body1 ]
  %y.done = icmp eq i256 %y, 0
  br i1 %y.done, label %small, label %body1

body1:
  %y.dec = sub i256 %y, 1
  %y.next = and i256 %y, %y.dec
  %c.next = add i256 %c, 1
  br label %loop1

small:
  %c.lt3 = icmp ult i256 %c, 3
  br i1 %c.lt3, label %exit, label %join

join:
  %wnz = icmp ne i256 %w, 0
  br i1 %wnz, label %loop2, label %exit

loop2:
  %z = phi i256 [ %x, %join ], [ %z.next, %body2 ]
  %d = phi i256 [ 0, %join ], [ %d.next, %body2 ]
  %z.done = icmp eq i256 %z, 0
  br i1 %z.done, label %count, label %body2

body2:
  %z.dec = sub i256 %z, 1
  %z.next = and i256 %z, %z.dec
  %d.next = add i256 %d, 1
  br label %loop2

count:
  %d.lt9 = icmp ult i256 %d, 9
  %r = zext i1 %d.lt9 to i256
  br label %exit

exit:
  %ret = phi i256 [ 7, %small ], [ 5, %join ], [ %r, %count ]
  ret i256 %ret
}

; O3-LABEL: define range(i256 0, 8) i256 @f(
; O3:         [[C:%.*]] = tail call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 %x)
; O3-NOT:     call {{.*}}@llvm.ctpop
; O3:       count.i:
; O3-NEXT:    [[LT9:%.*]] = icmp ult i256 [[C]], 9
; O3-NEXT:    [[R:%.*]] = zext i1 [[LT9]] to i256
; O3:         phi i256 [ 5, %join.i ], [ [[R]], %count.i ], [ 7, %0 ]
define i256 @f(i256 %x, i256 %w) {
  %r = call i256 @h(i256 %x, i256 %w)
  ret i256 %r
}

; O3-LABEL: define range(i256 100, 108) i256 @k(
; O3:         [[C:%.*]] = tail call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 %x)
; O3-NOT:     call {{.*}}@llvm.ctpop
; O3:       count.i:
; O3-NEXT:    [[LT9:%.*]] = icmp ult i256 [[C]], 9
; O3-NEXT:    [[R:%.*]] = select i1 [[LT9]], i256 101, i256 100
; O3:         phi i256 [ 105, %join.i ], [ [[R]], %count.i ], [ 107, %0 ]
define i256 @k(i256 %x, i256 %w) {
  %r = call i256 @h(i256 %x, i256 %w)
  %r100 = add i256 %r, 100
  ret i256 %r100
}

declare i256 @llvm.ctpop.i256(i256)
