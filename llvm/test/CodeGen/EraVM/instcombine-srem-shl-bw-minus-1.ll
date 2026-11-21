; RUN: opt -passes=instcombine -S < %s | FileCheck %s
; RUN: opt -passes='default<O3>' -S < %s | FileCheck %s --check-prefix=PIPE
; RUN: opt -passes='default<Oz>' -disable-loop-unrolling -S < %s | FileCheck %s --check-prefix=PIPE

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; InstCombine simplifyIRemMulShl matched `shl nsw X, 255` as `mul nsw X, 2^255`
; and kept NSW. `X <<s 255` and `X *s INT_MIN` are not equivalent: for X = -1
; the shl is INT_MIN without signed overflow, but -1 * INT_MIN overflows. With
; both operands "nsw" the srem was folded to `sub nsw 0, X`, so
; srem(INT_MIN, INT_MIN + 1) = -1 became 1.

; CHECK-LABEL: define i256 @srem_shl_mul(
; CHECK-NEXT:    %BO0 = mul nsw i256 %X, 57896044618658097711785492504343953926634992332820282019728792003956564819967
; CHECK-NEXT:    %BO1 = shl nsw i256 %X, 255
; CHECK-NEXT:    %r = srem i256 %BO1, %BO0
; CHECK-NEXT:    ret i256 %r
define i256 @srem_shl_mul(i256 noundef %X) {
  %BO0 = mul nsw i256 %X, 57896044618658097711785492504343953926634992332820282019728792003956564819967
  %BO1 = shl nsw i256 %X, 255
  %r = srem i256 %BO1, %BO0
  ret i256 %r
}

; The commuted form isn't folded either (it was folded to %BO0).
; CHECK-LABEL: define i256 @srem_mul_shl(
; CHECK-NEXT:    %BO0 = mul nsw i256 %X, 57896044618658097711785492504343953926634992332820282019728792003956564819967
; CHECK-NEXT:    %BO1 = shl nsw i256 %X, 255
; CHECK-NEXT:    %r = srem i256 %BO0, %BO1
; CHECK-NEXT:    ret i256 %r
define i256 @srem_mul_shl(i256 noundef %X) {
  %BO0 = mul nsw i256 %X, 57896044618658097711785492504343953926634992332820282019728792003956564819967
  %BO1 = shl nsw i256 %X, 255
  %r = srem i256 %BO0, %BO1
  ret i256 %r
}

; From Solidity inline assembly, after the range check of x:
;
;   if gt(add(x, 1), 1) { revert(0, 0) }
;   r := smod(shl(255, x), mul(x, 0x7fff...ff))
;
; x is -1 or 0, so the pipeline puts nsw on the shl and the mul, and InstCombine
; folded the srem to `sub nsw 0, x`: x = -1 gave 1 instead of -1.
; PIPE-LABEL: define {{.*}}i256 @guarded(
; PIPE:       %r = srem i256 %shl, %mul
; PIPE-NOT:   sub nsw i256 0, %x
define i256 @guarded(i256 %x) {
entry:
  %x1 = add i256 %x, 1
  %bad = icmp ugt i256 %x1, 1
  br i1 %bad, label %revert, label %body

body:
  %shl = shl i256 %x, 255
  %mul = mul i256 %x, 57896044618658097711785492504343953926634992332820282019728792003956564819967
  %r = srem i256 %shl, %mul
  ret i256 %r

revert:
  ret i256 0
}
