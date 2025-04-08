; RUN: opt -O3 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

declare void @llvm.evm.revert(ptr addrspace(1), i256)

define fastcc i256 @checked_mul(i256 %0) {
; CHECK-LABEL: checked_mul
; CHECK-NOT: @llvm.smul.with.overflow

entry:
  %multiplication_result = mul i256 100, %0
  %division_signed_is_divided_int_min = icmp eq i256 %multiplication_result, -57896044618658097711785492504343953926634992332820282019728792003956564819968
  br label %division_signed_non_overflow

division_signed_non_overflow:
  %division_signed_result_non_zero = sdiv i256 %multiplication_result, 100
  br label %division_signed_join

division_signed_join:
  %comparison_result = icmp eq i256 %0, %division_signed_result_non_zero
  %comparison_result_extended = zext i1 %comparison_result to i256
  %comparison_result3 = icmp eq i256 %comparison_result_extended, 0
  %comparison_result_extended4 = zext i1 %comparison_result3 to i256
  br i1 %comparison_result3, label %if_main, label %if_join

if_main:
  call void @llvm.evm.revert(ptr addrspace(1) noalias nocapture nofree noundef nonnull align 32 null, i256 36)
  unreachable

if_join:
  ret i256 %multiplication_result
}
