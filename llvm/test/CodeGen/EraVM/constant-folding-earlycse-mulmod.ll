; RUN: opt -early-cse -S < %s | FileCheck %s

target triple = "eravm"

declare i256 @__mulmod(i256, i256, i256) #0

define i256 @test_mulmod1() {
; CHECK-LABEL: @test_mulmod1
; CHECK-NEXT: ret i256 1

  ; Params are treated as unsigned values
  %res = call i256 @__mulmod(i256 -2, i256 -2, i256 3)
  ret i256 %res
}

define i256 @test_mulmod2() {
; CHECK-LABEL: @test_mulmod2
; CHECK-NEXT: ret i256 2

  %res = call i256 @__mulmod(i256 3, i256 17, i256 7)
  ret i256 %res
}

define i256 @test_mulmod3() {
; CHECK-LABEL: @test_mulmod3
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__mulmod(i256 undef, i256 17, i256 7)
  ret i256 %res
}

define i256 @test_mulmod4() {
; CHECK-LABEL: @test_mulmod4
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__mulmod(i256 17, i256 undef, i256 7)
  ret i256 %res
}

define i256 @test_mulmod5() {
; CHECK-LABEL: @test_mulmod5
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__mulmod(i256 3, i256 17, i256 undef)
  ret i256 %res
}

attributes #0 = { nounwind readnone willreturn }
