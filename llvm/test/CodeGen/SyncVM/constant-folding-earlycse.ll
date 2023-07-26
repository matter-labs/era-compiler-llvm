; RUN: opt -early-cse -S < %s | FileCheck %s

target triple = "syncvm"

declare i256 @__addmod(i256, i256, i256) #0
declare i256 @__mulmod(i256, i256, i256) #0
declare i256 @__signextend(i256, i256) #0
declare i256 @__exponent(i256, i256) #0

define i256 @test_addmod1() {
; CHECK-LABEL: @test_addmod1
; CHECK-NEXT: ret i256 1

  ; Params are treated as unsigned values
  %res = call i256 @__addmod(i256 -1, i256 2, i256 2)
  ret i256 %res
}

define i256 @test_addmod2() {
; CHECK-LABEL: @test_addmod2
; CHECK-NEXT: ret i256 4

  %res = call i256 @__addmod(i256 48, i256 1, i256 5)
  ret i256 %res
}

define i256 @test_addmod3() {
; CHECK-LABEL: @test_addmod3
; CHECK-NEXT: ret i256 3

  %res = call i256 @__addmod(i256 48, i256 undef, i256 5)
  ret i256 %res
}

define i256 @test_addmod4() {
; CHECK-LABEL: @test_addmod4
; CHECK-NEXT: ret i256 3

  %res = call i256 @__addmod(i256 undef, i256 48, i256 5)
  ret i256 %res
}

define i256 @test_addmod5() {
; CHECK-LABEL: @test_addmod5
; CHECK-NEXT: ret i256 0

  %res = call i256 @__addmod(i256 48, i256 1, i256 undef)
  ret i256 %res
}

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
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mulmod(i256 undef, i256 17, i256 7)
  ret i256 %res
}

define i256 @test_mulmod4() {
; CHECK-LABEL: @test_mulmod4
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mulmod(i256 17, i256 undef, i256 7)
  ret i256 %res
}

define i256 @test_mulmod5() {
; CHECK-LABEL: @test_mulmod5
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mulmod(i256 3, i256 17, i256 undef)
  ret i256 %res
}

define i256 @test_signextend1() {
; CHECK-LABEL: @test_signextend1
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__signextend(i256 0, i256 255)
  ret i256 %res
}

define i256 @test_signextend2() {
; CHECK-LABEL: @test_signextend2
; CHECK-NEXT: ret i256 32767

  %res = call i256 @__signextend(i256 1, i256 32767)
  ret i256 %res
}

define i256 @test_signextend3() {
; CHECK-LABEL: @test_signextend3
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__signextend(i256 undef, i256 32767)
  ret i256 %res
}

define i256 @test_signextend4() {
; CHECK-LABEL: @test_signextend4
; CHECK-NEXT: ret i256 0

  %res = call i256 @__signextend(i256 1, i256 undef)
  ret i256 %res
}

define i256 @test_signextend5() {
; CHECK-LABEL: @test_signextend5
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__signextend(i256 32, i256 undef)
  ret i256 %res
}

define i256 @test_signextend6() {
; CHECK-LABEL: @test_signextend6
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__signextend(i256 0, i256 33023)
  ret i256 %res
}

define i256 @test_exponent1() {
; CHECK-LABEL: @test_exponent1
; CHECK-NEXT: ret i256 0

  %res = call i256 @__exponent(i256 0, i256 10)
  ret i256 %res
}

define i256 @test_exponent2() {
; CHECK-LABEL: @test_exponent2
; CHECK-NEXT: ret i256 1

  %res = call i256 @__exponent(i256 2, i256 undef)
  ret i256 %res
}

define i256 @test_exponent3() {
; CHECK-LABEL: @test_exponent3
; CHECK-NEXT: ret i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968

  %res = call i256 @__exponent(i256 2, i256 255)
  ret i256 %res
}

define i256 @test_exponent4() {
; CHECK-LABEL: @test_exponent4
; CHECK-NEXT: ret i256 -26400738010602378953627016196889292963087978848325315750873680393886838386559

  %res = call i256 @__exponent(i256 307, i256 32)
  ret i256 %res
}

define i256 @test_exponent5() {
; CHECK-LABEL: @test_exponent5
; CHECK-NEXT: ret i256 0

  %res = call i256 @__exponent(i256 undef, i256 2)
  ret i256 %res
}

attributes #0 = {nounwind readnone willreturn}
