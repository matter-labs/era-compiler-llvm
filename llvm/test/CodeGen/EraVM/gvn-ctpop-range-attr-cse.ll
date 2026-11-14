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

; Three calls where CSE-ing the second into the first drops param 0's
; attributes (shrinking the leader's AttributeList from 3 sets to 2), and the
; third call still has 3 attribute sets. Without e343af777ef5,
; tryIntersectAttributes failed (2 != 3), aborting in patchReplacementInstruction
; with assertions enabled and keeping range(0, 9) in release builds.
; GVN-LABEL: define i256 @gvn_three_calls_shrink_attrsets(
; GVN:         %c1 = call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 %x)
; GVN-NOT:     call {{.*}}@llvm.ctpop
; GVN:       bb2:
; GVN-NEXT:    %lt9 = icmp ult i256 %c1, 9
; GVN-NEXT:    %r = zext i1 %lt9 to i256
;
; O3-LABEL: define range(i256 0, 9) i256 @gvn_three_calls_shrink_attrsets(
; O3:         %c1 = tail call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 %x)
; O3-NOT:     call {{.*}}@llvm.ctpop
; O3:       bb2:
; O3-NEXT:    %lt9 = icmp ult i256 %c1, 9
; O3-NEXT:    %r = zext i1 %lt9 to i256
define i256 @gvn_three_calls_shrink_attrsets(i256 %x, i1 %cond1, i1 %cond2) {
entry:
  %c1 = call range(i256 0, 9) i256 @llvm.ctpop.i256(i256 noundef %x)
  %u1 = icmp ult i256 %c1, 3
  %g1 = select i1 %cond1, i1 %u1, i1 false
  br i1 %g1, label %exit, label %bb1

bb1:
  %c2 = call range(i256 0, 9) i256 @llvm.ctpop.i256(i256 range(i256 0, 256) %x)
  %g2 = icmp eq i256 %c2, 4
  %s2 = select i1 %cond2, i1 %g2, i1 false
  br i1 %s2, label %exit, label %bb2

bb2:
  %c3 = call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 noundef %x)
  %lt9 = icmp ult i256 %c3, 9
  %r = zext i1 %lt9 to i256
  br label %exit

exit:
  %ret = phi i256 [ 7, %entry ], [ 8, %bb1 ], [ %r, %bb2 ]
  ret i256 %ret
}

; Two calls where only the first has a droppable parameter attribute (noundef):
; GVN must still CSE them and drop noundef on the kept call.
; GVN-LABEL: define i256 @gvn_diff_param_attr_cse(
; GVN-NEXT:    %c1 = call i256 @llvm.ctpop.i256(i256 %x)
; GVN-NEXT:    %sum = add i256 %c1, %c1
; GVN-NEXT:    ret i256 %sum
;
; O3-LABEL: define range(i256 0, 513) i256 @gvn_diff_param_attr_cse(
; O3-NEXT:    %c1 = tail call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 %x)
; O3-NEXT:    %sum = shl nuw nsw i256 %c1, 1
; O3-NEXT:    ret i256 %sum
define i256 @gvn_diff_param_attr_cse(i256 %x) {
  %c1 = call i256 @llvm.ctpop.i256(i256 noundef %x)
  %c2 = call i256 @llvm.ctpop.i256(i256 %x)
  %sum = add i256 %c1, %c2
  ret i256 %sum
}

declare i256 @llvm.ctpop.i256(i256)
