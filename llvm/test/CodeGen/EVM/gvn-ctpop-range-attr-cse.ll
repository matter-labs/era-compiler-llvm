; RUN: opt -passes=gvn -S < %s | FileCheck %s --check-prefix=GVN
; RUN: opt -passes='default<O3>' -S < %s | FileCheck %s --check-prefix=O3

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

; GVN-LABEL: define i256 @gvn_ctpop(
; GVN:         %c = call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 %x)
; GVN-NOT:     call {{.*}}@llvm.ctpop
; GVN:       count:
; GVN-NEXT:    %d.lt9 = icmp ult i256 %c, 9
; GVN-NEXT:    %r = zext i1 %d.lt9 to i256
; GVN:         %ret = phi i256 [ 7, %entry ], [ 5, %join ], [ %r, %count ]
;
; O3-LABEL: define range(i256 0, 8) i256 @gvn_ctpop(
; O3:         %c = tail call range(i256 0, 257) i256 @llvm.ctpop.i256(i256 %x)
; O3-NOT:     call {{.*}}@llvm.ctpop
; O3:       count:
; O3-NEXT:    %d.lt9 = icmp ult i256 %c, 9
; O3-NEXT:    %r = zext i1 %d.lt9 to i256
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
