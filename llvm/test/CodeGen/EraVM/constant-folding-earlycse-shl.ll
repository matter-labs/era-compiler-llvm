; RUN: opt -early-cse -S < %s | FileCheck %s

target triple = "eravm"

declare i256 @__shl(i256, i256) #0

define i256 @test_shl1() {
; CHECK-LABEL: @test_shl1
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__shl(i256 undef, i256 1)
  ret i256 %res
}

define i256 @test_shl2() {
; CHECK-LABEL: @test_shl2
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__shl(i256 1, i256 undef)
  ret i256 %res
}

define i256 @test_shl3() {
; CHECK-LABEL: @test_shl3
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 0, i256 0)
  ret i256 %res
}

define i256 @test_shl4() {
; CHECK-LABEL: @test_shl4
; CHECK-NEXT: ret i256 1

  %res = call i256 @__shl(i256 0, i256 1)
  ret i256 %res
}

define i256 @test_shl5() {
; CHECK-LABEL: @test_shl5
; CHECK-NEXT: ret i256 37670211480306196047687443673641227745170897112008692523754794019498533073987

  %res = call i256 @__shl(i256 0, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shl6() {
; CHECK-LABEL: @test_shl6
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__shl(i256 0, i256 -1)
  ret i256 %res
}

define i256 @test_shl7() {
; CHECK-LABEL: @test_shl7
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 1, i256 0)
  ret i256 %res
}

define i256 @test_shl8() {
; CHECK-LABEL: @test_shl8
; CHECK-NEXT: ret i256 2

  %res = call i256 @__shl(i256 1, i256 1)
  ret i256 %res
}

define i256 @test_shl9() {
; CHECK-LABEL: @test_shl9
; CHECK-NEXT: ret i256 -40451666276703803328196097661405452362928190441623178991947995968916063491962

  %res = call i256 @__shl(i256 1, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shl10() {
; CHECK-LABEL: @test_shl10
; CHECK-NEXT: ret i256 -2

  %res = call i256 @__shl(i256 1, i256 -1)
  ret i256 %res
}

define i256 @test_shl11() {
; CHECK-LABEL: @test_shl11
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 40, i256 0)
  ret i256 %res
}

define i256 @test_shl12() {
; CHECK-LABEL: @test_shl12
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 40, i256 0)
  ret i256 %res
}

define i256 @test_shl13() {
; CHECK-LABEL: @test_shl13
; CHECK-NEXT: ret i256 1099511627776

  %res = call i256 @__shl(i256 40, i256 1)
  ret i256 %res
}

define i256 @test_shl14() {
; CHECK-LABEL: @test_shl14
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 40, i256 0)
  ret i256 %res
}

define i256 @test_shl15() {
; CHECK-LABEL: @test_shl15
; CHECK-NEXT: ret i256 40063763421114410033923212107166246724796859226574628379196082273060414029824

  %res = call i256 @__shl(i256 40, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shl16() {
; CHECK-LABEL: @test_shl16
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__shl(i256 40, i256 -1)
  ret i256 %res
}

define i256 @test_shl17() {
; CHECK-LABEL: @test_shl17
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 171, i256 0)
  ret i256 %res
}

define i256 @test_shl18() {
; CHECK-LABEL: @test_shl18
; CHECK-NEXT: ret i256 2993155353253689176481146537402947624255349848014848

  %res = call i256 @__shl(i256 171, i256 1)
  ret i256 %res
}

define i256 @test_shl19() {
; CHECK-LABEL: @test_shl19
; CHECK-NEXT: ret i256 -19177177688971274584416941610210820178639383122214864933301741884673332609024

  %res = call i256 @__shl(i256 171, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shl20() {
; CHECK-LABEL: @test_shl20
; CHECK-NEXT: ret i256 -2993155353253689176481146537402947624255349848014848

  %res = call i256 @__shl(i256 171, i256 115792089237316195423570985008687907853269984665640564039457584007913129639935)
  ret i256 %res
}

define i256 @test_shl121() {
; CHECK-LABEL: @test_shl121
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 255, i256 0)
  ret i256 %res
}

define i256 @test_shl122() {
; CHECK-LABEL: @test_shl122
; CHECK-NEXT: ret i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968

  %res = call i256 @__shl(i256 255, i256 1)
  ret i256 %res
}

define i256 @test_shl23() {
; CHECK-LABEL: @test_shl23
; CHECK-NEXT: ret i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968

  %res = call i256 @__shl(i256 255, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shl24() {
; CHECK-LABEL: @test_shl24
; CHECK-NEXT: ret i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968

  %res = call i256 @__shl(i256 255, i256 -1)
  ret i256 %res
}

define i256 @test_shl25() {
; CHECK-LABEL: @test_shl25
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 256, i256 0)
  ret i256 %res
}

define i256 @test_shl26() {
; CHECK-LABEL: @test_shl26
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 256, i256 1)
  ret i256 %res
}

define i256 @test_shl27() {
; CHECK-LABEL: @test_shl27
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 256, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shl28() {
; CHECK-LABEL: @test_shl28
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 35242523534534534233424343343443, i256 0)
  ret i256 %res
}

define i256 @test_shl29() {
; CHECK-LABEL: @test_shl29
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 35242523534534534233424343343443, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shl30() {
; CHECK-LABEL: @test_shl30
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 35242523534534534233424343343443, i256 -1)
  ret i256 %res
}

define i256 @test_shl31() {
; CHECK-LABEL: @test_shl31
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 -1, i256 0)
  ret i256 %res
}

define i256 @test_shl32() {
; CHECK-LABEL: @test_shl32
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 -1, i256 1)
  ret i256 %res
}

define i256 @test_shl33() {
; CHECK-LABEL: @test_shl33
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 -1, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shl34() {
; CHECK-LABEL: @test_shl34
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shl(i256 -1, i256 -1)
  ret i256 %res
}

attributes #0 = { nounwind readnone willreturn }
