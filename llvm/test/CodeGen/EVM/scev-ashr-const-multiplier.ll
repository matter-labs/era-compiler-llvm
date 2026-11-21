; RUN: opt -S -passes='default<O3>' < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

define { i256, i256 } @scev_ashr_const_multiplier(i256 %x) {
; CHECK-LABEL: define {{.*}}@scev_ashr_const_multiplier(
; CHECK:       mul i256 %{{.*}}, 57896044618658097711785492504343953926634992332820282019728792003956564819967
; CHECK:       %d = udiv i256 %{{.*}}, 57896044618658097711785492504343953926634992332820282019728792003956564819967
; CHECK-NEXT:  %ret0 = insertvalue { i256, i256 } poison, i256 %d, 0
;
entry:
  br label %loop

loop:
  %i = phi i256 [ 0, %entry ], [ %i.next, %loop ]
  %j = phi i256 [ %x, %entry ], [ %j.next, %loop ]
  %acc = phi i256 [ 0, %entry ], [ %acc.next, %loop ]
  %cur = sub i256 %j, %i
  %a = and i256 %cur, 16382
  %neg = sub i256 0, %a
  %res = and i256 %neg, 268431360
  %z = lshr i256 %a, 1
  %m = mul i256 %z, 57896044618658097711785492504343953926634992332820282019728792003956564819967
  %sum = add i256 %res, %m
  %acc.next = add i256 %acc, %sum
  %i.next = add nuw nsw i256 %i, 1
  %j.next = add i256 %j, 1
  %cond = icmp ult i256 %i.next, 2
  br i1 %cond, label %loop, label %exit

exit:
  %d = udiv i256 %m, 57896044618658097711785492504343953926634992332820282019728792003956564819967
  %ret0 = insertvalue { i256, i256 } poison, i256 %d, 0
  %ret1 = insertvalue { i256, i256 } %ret0, i256 %acc.next, 1
  ret { i256, i256 } %ret1
}
