; RUN: opt -early-cse -S < %s | FileCheck %s

target triple = "syncvm"

declare i256 @__mulmod(i256, i256, i256) #0
declare i256 @__mulmod_replaced(i256, i256, i256) #0

; CHECK-LABEL: @test_mulmod
define i256 @test_mulmod(i256 %x, i256 %y) {
  %res = call i256 @__mulmod(i256 %x, i256 %y, i256 3)
  ret i256 %res
}

; CHECK-LABEL: @test_mulmod2
define i256 @test_mulmod2() {
  %res = call i256 @__mulmod(i256 1023, i256 110, i256 39)
  ret i256 %res
}
