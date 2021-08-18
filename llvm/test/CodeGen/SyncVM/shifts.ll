; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: shlc
define i256 @shlc(i256 %par) nounwind {
; CHECK: shl r1, #10, r1
  %1 = shl i256 %par, 10
  ret i256 %1
}

; CHECK-LABEL: shl2
define i256 @shl2(i256 %par, i256 %sft) nounwind {
; CHECK: shl r1, r2, r1
  %1 = shl i256 %par, %sft
  ret i256 %1
}

; CHECK-LABEL: shrc
define i256 @shrc(i256 %par) nounwind {
; CHECK: shr r1, #10, r1
  %1 = lshr i256 %par, 10
  ret i256 %1
}

; CHECK-LABEL: shr2
define i256 @shr2(i256 %par, i256 %sft) nounwind {
; CHECK: shr r1, r2, r1
  %1 = lshr i256 %par, %sft
  ret i256 %1
}

; CHECK-LABEL: rol:
define i256 @rol(i256 %val, i256 %amt) {
; CHECK: rol r1, r2, r1
  %mod = urem i256 %amt, 256
  %inv = sub i256 256, %mod
  %parta = shl i256 %val, %mod
  %partb = lshr i256 %val, %inv
  %rotl = or i256 %parta, %partb
  ret i256 %rotl
}

; CHECK-LABEL: ror:
define i256 @ror(i256 %val, i256 %amt) {
; CHECK: ror r1, r2, r1
  %mod = urem i256 %amt, 256
  %inv = sub i256 256, %mod
  %parta = lshr i256 %val, %mod
  %partb = shl i256 %val, %inv
  %rotr = or i256 %parta, %partb
  ret i256 %rotr
}
