; UNSUPPORTED: evm

; RUN: opt -O2 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test_div1(i256 %a) {
; CHECK-LABEL: @test_div1
; CHECK: ret i256 0
  %res = call i256 @__div(i256 %a, i256 0)
  ret i256 %res
}

define i256 @test_div2(i256 %a) {
; CHECK-LABEL: @test_div2
; CHECK: ret i256 0
  %res = call i256 @__div(i256 0, i256 %a)
  ret i256 %res
}

define i256 @test_div3(i256 %a) {
; CHECK-LABEL: @test_div3
; CHECK: ret i256 %a
  %res = call i256 @__div(i256 %a, i256 1)
  ret i256 %res
}

define i256 @test_sdiv1(i256 %a) {
; CHECK-LABEL: @test_sdiv1
; CHECK: ret i256 0
  %res = call i256 @__sdiv(i256 %a, i256 0)
  ret i256 %res
}

define i256 @test_sdiv2(i256 %a) {
; CHECK-LABEL: @test_sdiv2
; CHECK: ret i256 0
  %res = call i256 @__sdiv(i256 0, i256 %a)
  ret i256 %res
}

define i256 @test_sdiv3(i256 %a) {
; CHECK-LABEL: @test_sdiv3
; CHECK: ret i256 %a
  %res = call i256 @__sdiv(i256 %a, i256 1)
  ret i256 %res
}

define i256 @test_mod1(i256 %a) {
; CHECK-LABEL: @test_mod1
; CHECK: ret i256 0
  %res = call i256 @__mod(i256 %a, i256 0)
  ret i256 %res
}

define i256 @test_mod2(i256 %a) {
; CHECK-LABEL: @test_mod2
; CHECK: ret i256 0
  %res = call i256 @__mod(i256 0, i256 %a)
  ret i256 %res
}

define i256 @test_mod3(i256 %a) {
; CHECK-LABEL: @test_mod3
; CHECK: ret i256 0
  %res = call i256 @__mod(i256 %a, i256 1)
  ret i256 %res
}

define i256 @test_smod1(i256 %a) {
; CHECK-LABEL: @test_smod1
; CHECK: ret i256 0
  %res = call i256 @__smod(i256 %a, i256 0)
  ret i256 %res
}

define i256 @test_smod2(i256 %a) {
; CHECK-LABEL: @test_smod2
; CHECK: ret i256 0
  %res = call i256 @__smod(i256 0, i256 %a)
  ret i256 %res
}

define i256 @test_smod3(i256 %a) {
; CHECK-LABEL: @test_smod3
; CHECK: ret i256 0
  %res = call i256 @__smod(i256 %a, i256 1)
  ret i256 %res
}

define i256 @test_exp1(i256 %a) {
; CHECK-LABEL: @test_exp1
; CHECK: ret i256 1
  %res = call i256 @__exp(i256 1, i256 %a)
  ret i256 %res
}

define i256 @test_exp2(i256 %a) {
; CHECK-LABEL: @test_exp2
; CHECK: ret i256 %a
  %res = call i256 @__exp(i256 %a, i256 1)
  ret i256 %res
}

define i256 @test_exp3(i256 %a) {
; CHECK-LABEL: @test_exp3
; CHECK: mul nuw nsw i256 %a, 3
; CHECK-NEXT: icmp ugt i256 {{.*}}, 255
; CHECK-NEXT: shl nuw i256 1, {{.*}}
; CHECK-NEXT: select
  %res = call i256 @__exp(i256 8, i256 %a)
  ret i256 %res
}

define i256 @test_exp4(i256 %a) {
; CHECK-LABEL: @test_exp4
; CHECK: icmp ugt i256 %a, 255
; CHECK-NEXT: shl nuw i256 1, %a
; CHECK-NEXT: select
  %res = call i256 @__exp(i256 2, i256 %a)
  ret i256 %res
}

define i256 @test_shl1(i256 %x) {
; CHECK-LABEL: @test_shl1
; CHECK: ret i256 %x
  %res = call i256 @__shl(i256 0, i256 %x)
  ret i256 %res
}

define i256 @test_shl2(i256 %x) {
; CHECK-LABEL: @test_shl2
; CHECK: ret i256 0
  %res = call i256 @__shl(i256 256, i256 %x)
  ret i256 %res
}

define i256 @test_shl3(i256 %x) {
; CHECK-LABEL: @test_shl3
; CHECK: ret i256 0
  %res = call i256 @__shl(i256 %x, i256 0)
  ret i256 %res
}

define i256 @test_shr1(i256 %x) {
; CHECK-LABEL: @test_shr1
; CHECK: ret i256 %x
  %res = call i256 @__shr(i256 0, i256 %x)
  ret i256 %res
}

define i256 @test_shr2(i256 %x) {
; CHECK-LABEL: @test_shr2
; CHECK: ret i256 0
  %res = call i256 @__shr(i256 256, i256 %x)
  ret i256 %res
}

define i256 @test_shr3(i256 %x) {
; CHECK-LABEL: @test_shr3
; CHECK: ret i256 0
  %res = call i256 @__shr(i256 %x, i256 0)
  ret i256 %res
}

define i256 @test_sar1(i256 %x) {
; CHECK-LABEL: @test_sar1
; CHECK: ret i256 %x
  %res = call i256 @__sar(i256 0, i256 %x)
  ret i256 %res
}

define i256 @test_sar2(i256 %x) {
; CHECK-LABEL: @test_sar2
; CHECK: ret i256 0
  %res = call i256 @__sar(i256 %x, i256 0)
  ret i256 %res
}

declare i256 @__div(i256 %a, i256 %b) #0
declare i256 @__sdiv(i256 %a, i256 %b) #0
declare i256 @__mod(i256 %a, i256 %b) #0
declare i256 @__smod(i256 %a, i256 %b) #0
declare i256 @__exp(i256 %value, i256 %exp) #0
declare i256 @__shl(i256, i256) #0
declare i256 @__shr(i256, i256) #0
declare i256 @__sar(i256, i256) #0
declare i256 @__byte(i256, i256) #0

attributes #0 = {nounwind readnone willreturn}
