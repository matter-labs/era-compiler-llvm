; RUN: opt -passes=indvars -S < %s | FileCheck %s --check-prefix=INDVARS
; RUN: opt -passes='default<Oz>' -disable-loop-unrolling -S < %s | FileCheck %s --check-prefix=OZ

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; SCEV createAddRecFromPHI rewrote phi(Start, BEValue) into an add recurrence
; even when the rewritten expression has a udiv that may divide by zero. The
; UB of the division was dropped, and the result was a wrong SCEV.

; From the Solidity loop
;
;   uint p = 0; uint k = 1; uint i = a; uint j = a;
;   while (true) {
;     if (p > 3) return 7;
;     if (k > b) return 5;
;     unchecked { p = k / (i - j); k++; i++; j++; }
;   }
;
; i and j are the same IV, so the divisor is SCEV 0, and p became
; {0,+,1} /u 0. IndVarSimplify folded the first-iteration test `p > 3` to
; true, and f(a, b) returned 7 (zksolc -Oz, Yul codegen) instead of 5 for
; b == 0 or a division-by-zero panic (18 here) otherwise.
; INDVARS-LABEL: define i256 @scev_udiv_zero(
; INDVARS:       loop:
; INDVARS-NEXT:    br i1 false, label %exit, label %check.k
; OZ-LABEL:      define {{.*}}i256 @scev_udiv_zero(
; OZ:              %k.gt.b = icmp eq i256 %b, 0
; OZ-NEXT:         %common.ret.op = select i1 %k.gt.b, i256 5, i256 18
; OZ-NEXT:         ret i256 %common.ret.op
define i256 @scev_udiv_zero(i256 %a, i256 %b) {
entry:
  br label %loop

loop:
  %p = phi i256 [ 0, %entry ], [ %p.next, %latch ]
  %k = phi i256 [ 1, %entry ], [ %k.next, %latch ]
  %i = phi i256 [ %a, %entry ], [ %i.next, %latch ]
  %j = phi i256 [ %a, %entry ], [ %j.next, %latch ]
  %p.gt3 = icmp ugt i256 %p, 3
  br i1 %p.gt3, label %exit, label %check.k

check.k:
  %k.gt.b = icmp ugt i256 %k, %b
  br i1 %k.gt.b, label %exit, label %check.div

check.div:
  %div.zero = icmp eq i256 %i, %j
  br i1 %div.zero, label %panic, label %latch

latch:
  %diff = sub i256 %i, %j
  %p.next = udiv i256 %k, %diff
  %k.next = add i256 %k, 1
  %i.next = add i256 %i, 1
  %j.next = add i256 %j, 1
  br label %loop

exit:
  %r = phi i256 [ 7, %loop ], [ 5, %check.k ]
  ret i256 %r

panic:
  ret i256 18
}

; The BEValue has no udiv by a value that may be zero, but the rewritten
; expression has -83 /u {0,+,1} (upstream pr123550.ll). The loop runs once,
; so the exit value is srem(729259140, -173) = 130, not 729259140.
; INDVARS-LABEL: define i32 @shifted_udiv(
; INDVARS:         ret i32 130
define i32 @shifted_udiv() {
bb:
  br label %loop

loop:
  %phi = phi i32 [ -173, %bb ], [ %sub, %loop ]
  %iv2 = phi i32 [ 1, %bb ], [ %iv2.inc, %loop ]
  %srem = srem i32 729259140, %phi
  %trunc = trunc i32 %iv2 to i8
  %urem = urem i8 -83, %trunc
  %zext = zext i8 %urem to i32
  %sub = sub i32 0, %zext
  %iv2.inc = add i32 %iv2, 1
  %icmp = icmp eq i32 %zext, 0
  br i1 %icmp, label %exit, label %loop

exit:
  %srem.lcssa = phi i32 [ %srem, %loop ]
  ret i32 %srem.lcssa
}

; The same through the pipeline: @f is simplified on its own first and is
; inlined into @shifted_udiv_entry afterwards, where the copy with %ivs = 1
; (one iteration, %phi = -173) meets SCEV. For n == 0 the loop passes
; srem(729259140, -172) = 124 to @use and returns it; it returned 729259140.
; OZ-LABEL:      define {{.*}}i256 @shifted_udiv_entry(
; OZ:              call fastcc void @use(i32 124)
; OZ:              %s = phi i32 [ 124, %loop.i.preheader ], [ %srem.i{{[0-9]*}}, %loop.i{{[0-9]*}} ]
define internal void @use(i32 %v) noinline {
  %sw = sext i32 %v to i256
  %p = inttoptr i256 256 to ptr addrspace(1)
  %old = load i256, ptr addrspace(1) %p, align 32
  %a = add i256 %old, %sw
  store i256 %a, ptr addrspace(1) %p, align 32
  ret void
}

define internal i32 @f(i8 %c, i32 %start, i32 %ivs, i32 %k) {
bb:
  br label %loop

loop:
  %phi = phi i32 [ %start, %bb ], [ %sub, %loop ]
  %iv2 = phi i32 [ %ivs, %bb ], [ %iv2.inc, %loop ]
  %phi1 = add i32 %phi, 1
  %srem = srem i32 729259140, %phi1
  call void @use(i32 %srem)
  %trunc = trunc i32 %iv2 to i8
  %urem = urem i8 %c, %trunc
  %zext = zext i8 %urem to i32
  %sub = sub i32 %k, %zext
  %iv2.inc = add i32 %iv2, 1
  %icmp = icmp eq i32 %iv2, 1
  br i1 %icmp, label %exit, label %loop

exit:
  %srem.lcssa = phi i32 [ %srem, %loop ]
  ret i32 %srem.lcssa
}

define i256 @shifted_udiv_entry(i256 %n, i256 %a1) {
bb:
  %cond = icmp eq i256 %n, 0
  br i1 %cond, label %first, label %other

first:
  %x = call i32 @f(i8 -83, i32 -173, i32 1, i32 0)
  br label %ret

other:
  %t = trunc i256 %a1 to i32
  %y = call i32 @f(i8 -83, i32 -173, i32 %t, i32 0)
  br label %ret

ret:
  %s = phi i32 [ %x, %first ], [ %y, %other ]
  %r = sext i32 %s to i256
  ret i256 %r
}
