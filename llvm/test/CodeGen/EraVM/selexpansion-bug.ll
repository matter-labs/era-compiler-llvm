; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare i32 @__personality()

declare i256 @__signextend(i256, i256) #3

; Function Attrs: noreturn
; CHECK-LABEL: __entry
define i256 @__entry(i256 %0, i256 %1, i1 %2) local_unnamed_addr #1 personality i32 ()* @__personality {
entry:
  tail call fastcc void @__selector()
  unreachable
}

; Function Attrs: noreturn
define private fastcc void @__selector() unnamed_addr #1 personality i32 ()* @__personality {
entry:
  %sign_extend_call = call i256 @__signextend(i256 0, i256 undef)
  %sign_extend_call30 = call i256 @__signextend(i256 0, i256 undef)
  br i1 undef, label %division_signed_join, label %division_signed_non_overflow

division_signed_non_overflow:                     ; preds = %entry
  %division_signed_result_non_zero = sdiv i256 %sign_extend_call, %sign_extend_call30
  br label %division_signed_join

division_signed_join:                             ; preds = %division_signed_non_overflow, %entry
  %storemerge = phi i256 [ %division_signed_result_non_zero, %division_signed_non_overflow ], [ -57896044618658097711785492504343953926634992332820282019728792003956564819968, %entry ]
  tail call fastcc void @abi_encode_int8(i256 %storemerge)
  unreachable
}

define private fastcc void @abi_encode_int8(i256 %0) unnamed_addr personality i32 ()* @__personality {
entry:
  unreachable
}

attributes #0 = { noprofile }
attributes #1 = { noreturn }
attributes #2 = { noreturn nounwind }
attributes #3 = {nounwind readnone willreturn}
