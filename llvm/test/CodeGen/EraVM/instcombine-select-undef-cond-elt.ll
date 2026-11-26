; RUN: opt -passes=instcombine -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; An undef element of a constant select condition may pick either operand,
; so it must not be treated as true, which made the false operand undemanded
; in that lane and folded the lane to poison.

define <2 x i256> @sel(<2 x i256> %val) {
; CHECK-LABEL: @sel(
; CHECK-NEXT:    [[R:%.*]] = select <2 x i1> <i1 undef, i1 true>, <2 x i256> <i256 1, i256 1>, <2 x i256> [[VAL:%.*]]
; CHECK-NEXT:    ret <2 x i256> [[R]]
  %r = select <2 x i1> <i1 undef, i1 true>, <2 x i256> <i256 1, i256 1>, <2 x i256> %val
  ret <2 x i256> %r
}

define i256 @sel_extract(<2 x i256> %val) {
; CHECK-LABEL: @sel_extract(
; CHECK:         ret i256 {{%|[0-9]}}
  %r = select <2 x i1> <i1 undef, i1 true>, <2 x i256> <i256 1, i256 1>, <2 x i256> %val
  %e = extractelement <2 x i256> %r, i32 0
  ret i256 %e
}

define i256 @sel_insert_extract(i256 %x) {
; CHECK-LABEL: @sel_insert_extract(
; CHECK:         ret i256 {{%|[0-9]}}
  %v = insertelement <2 x i256> poison, i256 %x, i32 0
  %r = select <2 x i1> <i1 undef, i1 true>, <2 x i256> <i256 7, i256 7>, <2 x i256> %v
  %e = extractelement <2 x i256> %r, i32 0
  ret i256 %e
}
