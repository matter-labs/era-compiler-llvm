; RUN: opt -early-cse -S < %s | FileCheck %s

target triple = "eravm"

declare i256 @__shr(i256, i256) #0

define i256 @test_shr1() {
; CHECK-LABEL: @test_shr1
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__shr(i256 undef, i256 1)
  ret i256 %res
}

define i256 @test_shr2() {
; CHECK-LABEL: @test_shr2
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__shr(i256 1, i256 undef)
  ret i256 %res
}

define i256 @test_shr3() {
; CHECK-LABEL: @test_shr3
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 0, i256 0)
  ret i256 %res
}

define i256 @test_shr4() {
; CHECK-LABEL: @test_shr4
; CHECK-NEXT: ret i256 1

  %res = call i256 @__shr(i256 0, i256 1)
  ret i256 %res
}

define i256 @test_shr5() {
; CHECK-LABEL: @test_shr5
; CHECK-NEXT: ret i256 37670211480306196047687443673641227745170897112008692523754794019498533073987

  %res = call i256 @__shr(i256 0, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shr6() {
; CHECK-LABEL: @test_shr6
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__shr(i256 0, i256 -1)
  ret i256 %res
}

define i256 @test_shr7() {
; CHECK-LABEL: @test_shr7
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 1, i256 0)
  ret i256 %res
}

define i256 @test_shr8() {
; CHECK-LABEL: @test_shr8
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 1, i256 1)
  ret i256 %res
}

define i256 @test_shr9() {
; CHECK-LABEL: @test_shr9
; CHECK-NEXT: ret i256 18835105740153098023843721836820613872585448556004346261877397009749266536993

  %res = call i256 @__shr(i256 1, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shr10() {
; CHECK-LABEL: @test_shr10
; CHECK-NEXT: ret i256 57896044618658097711785492504343953926634992332820282019728792003956564819967

  %res = call i256 @__shr(i256 1, i256 -1)
  ret i256 %res
}

define i256 @test_shr11() {
; CHECK-LABEL: @test_shr11
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 40, i256 0)
  ret i256 %res
}

define i256 @test_shr12() {
; CHECK-LABEL: @test_shr12
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 40, i256 1)
  ret i256 %res
}

define i256 @test_shr13() {
; CHECK-LABEL: @test_shr13
; CHECK-NEXT: ret i256 34260857756004221344198816653844212619488550546528761082076738620

  %res = call i256 @__shr(i256 40, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shr14() {
; CHECK-LABEL: @test_shr14
; CHECK-NEXT: ret i256 105312291668557186697918027683670432318895095400549111254310977535

  %res = call i256 @__shr(i256 40, i256 -1)
  ret i256 %res
}

define i256 @test_shr15() {
; CHECK-LABEL: @test_shr15
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 171, i256 0)
  ret i256 %res
}

define i256 @test_shr16() {
; CHECK-LABEL: @test_shr16
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 171, i256 1)
  ret i256 %res
}

define i256 @test_shr17() {
; CHECK-LABEL: @test_shr17
; CHECK-NEXT: ret i256 12585451483284036284784745

  %res = call i256 @__shr(i256 171, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shr18() {
; CHECK-LABEL: @test_shr18
; CHECK-NEXT: ret i256 38685626227668133590597631

  %res = call i256 @__shr(i256 171, i256 -1)
  ret i256 %res
}

define i256 @test_shr19() {
; CHECK-LABEL: @test_shr19
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 255, i256 0)
  ret i256 %res
}

define i256 @test_shr20() {
; CHECK-LABEL: @test_shr20
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 255, i256 1)
  ret i256 %res
}

define i256 @test_shr21() {
; CHECK-LABEL: @test_shr21
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 255, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shr22() {
; CHECK-LABEL: @test_shr22
; CHECK-NEXT: ret i256 1

  %res = call i256 @__shr(i256 255, i256 -1)
  ret i256 %res
}

define i256 @test_shr23() {
; CHECK-LABEL: @test_shr23
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 256, i256 0)
  ret i256 %res
}

define i256 @test_shr24() {
; CHECK-LABEL: @test_shr24
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 256, i256 1)
  ret i256 %res
}

define i256 @test_shr25() {
; CHECK-LABEL: @test_shr25
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 256, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shr26() {
; CHECK-LABEL: @test_shr26
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 256, i256 -1)
  ret i256 %res
}

define i256 @test_shr27() {
; CHECK-LABEL: @test_shr27
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 35242523534534534233424343343443, i256 0)
  ret i256 %res
}

define i256 @test_shr28() {
; CHECK-LABEL: @test_shr28
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 35242523534534534233424343343443, i256 1)
  ret i256 %res
}

define i256 @test_shr29() {
; CHECK-LABEL: @test_shr29
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 35242523534534534233424343343443, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shr30() {
; CHECK-LABEL: @test_shr30
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 35242523534534534233424343343443, i256 -1)
  ret i256 %res
}

define i256 @test_shr31() {
; CHECK-LABEL: @test_shr31
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 -1, i256 0)
  ret i256 %res
}

define i256 @test_shr32() {
; CHECK-LABEL: @test_shr32
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 -1, i256 1)
  ret i256 %res
}

define i256 @test_shr33() {
; CHECK-LABEL: @test_shr33
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 -1, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_shr34() {
; CHECK-LABEL: @test_shr34
; CHECK-NEXT: ret i256 0

  %res = call i256 @__shr(i256 -1, i256 -1)
  ret i256 %res
}

attributes #0 = { nounwind readnone willreturn }
