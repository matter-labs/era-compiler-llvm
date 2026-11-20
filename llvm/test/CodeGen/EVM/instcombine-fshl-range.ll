; RUN: opt -passes='default<O3>' -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

declare i256 @llvm.fshl.i256(i256, i256, i256)

; Check that InstCombine drops the range attribute on llvm.fshl when
; SimplifyDemandedBits shrinks `or i256 %x, -64` to `or i256 %x, 64`,
; so that the subsequent sign-extension is not folded away.
define i256 @test_fshl_range_dropped(i256 %x, i256 %y) {
; CHECK-LABEL: @test_fshl_range_dropped(
; CHECK-NEXT:    [[A:%.*]] = or i256 [[X:%.*]], 64
; CHECK-NEXT:    [[V:%.*]] = tail call i256 @llvm.fshl.i256(i256 [[A]], i256 [[Y:%.*]], i256 1)
; CHECK-NEXT:    [[SHL:%.*]] = shl i256 [[V]], 248
; CHECK-NEXT:    [[S:%.*]] = ashr exact i256 [[SHL]], 248
; CHECK-NEXT:    ret i256 [[S]]
;
  %a = or i256 %x, -64
  %v = call range(i256 -128, 0) i256 @llvm.fshl.i256(i256 %a, i256 %y, i256 1)
  %t = trunc i256 %v to i8
  %s = sext i8 %t to i256
  ret i256 %s
}
