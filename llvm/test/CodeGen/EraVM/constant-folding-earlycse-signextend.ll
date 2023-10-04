; RUN: opt -early-cse -S < %s | FileCheck %s

target triple = "eravm"

declare i256 @__signextend(i256, i256) #0

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
; CHECK-NEXT: ret i256 poison

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
; CHECK-NEXT: ret i256 0

  %res = call i256 @__signextend(i256 0, i256 0)
  ret i256 %res
}

define i256 @test_signextend7() {
; CHECK-LABEL: @test_signextend7
; CHECK-NEXT: ret i256 1

  %res = call i256 @__signextend(i256 0, i256 1)
  ret i256 %res
}

define i256 @test_signextend8() {
; CHECK-LABEL: @test_signextend8
; CHECK-NEXT: ret i256 67

  %res = call i256 @__signextend(i256 0, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_signextend9() {
; CHECK-LABEL: @test_signextend9
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__signextend(i256 0, i256 -1)
  ret i256 %res
}

define i256 @test_signextend10() {
; CHECK-LABEL: @test_signextend10
; CHECK-NEXT: ret i256 0

  %res = call i256 @__signextend(i256 1, i256 0)
  ret i256 %res
}

define i256 @test_signextend11() {
; CHECK-LABEL: @test_signextend11
; CHECK-NEXT: ret i256 1

  %res = call i256 @__signextend(i256 1, i256 1)
  ret i256 %res
}

define i256 @test_signextend12() {
; CHECK-LABEL: @test_signextend12
; CHECK-NEXT: ret i256 -31677

  %res = call i256 @__signextend(i256 1, i256 37670211480306196047687443673641227745170897112008692523754794019498533094467)
  ret i256 %res
}

define i256 @test_signextend13() {
; CHECK-LABEL: @test_signextend13
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__signextend(i256 1, i256 -1)
  ret i256 %res
}

define i256 @test_signextend14() {
; CHECK-LABEL: @test_signextend14
; CHECK-NEXT: ret i256 0

  %res = call i256 @__signextend(i256 17, i256 0)
  ret i256 %res
}

define i256 @test_signextend15() {
; CHECK-LABEL: @test_signextend15
; CHECK-NEXT: ret i256 1

  %res = call i256 @__signextend(i256 4, i256 1)
  ret i256 %res
}

define i256 @test_signextend16() {
; CHECK-LABEL: @test_signextend16
; CHECK-NEXT: ret i256 9111590707254702215554908033119796504552448929051039916

  %res = call i256 @__signextend(i256 22, i256 9111590707254702215554908033119796504552448929051039916)
  ret i256 %res
}

define i256 @test_signextend17() {
; CHECK-LABEL: @test_signextend17
; CHECK-NEXT: ret i256 2936025693725350223284924577414370008619180

  %res = call i256 @__signextend(i256 17, i256 25236770892255973364820642850062731514599596)
  ret i256 %res
}

define i256 @test_signextend18() {
; CHECK-LABEL: @test_signextend18
; CHECK-NEXT: ret i256 -7745860242270075226386909265533604515253681414968584020

  %res = call i256 @__signextend(i256 22, i256 16774068411584146507346643168871342422646144539969050796)
  ret i256 %res
}

define i256 @test_signextend19() {
; CHECK-LABEL: @test_signextend19
; CHECK-NEXT: ret i256 -5426753755723633454790969774828765556123476

  %res = call i256 @__signextend(i256 17, i256 93509753590725542020426220953279705230436762145237986916280948908)
  ret i256 %res
}

define i256 @test_signextend20() {
; CHECK-LABEL: @test_signextend20
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__signextend(i256 25, i256 -1)
  ret i256 %res
}

define i256 @test_signextend21() {
; CHECK-LABEL: @test_signextend21
; CHECK-NEXT: ret i256 0

  %res = call i256 @__signextend(i256 255, i256 0)
  ret i256 %res
}

define i256 @test_signextend22() {
; CHECK-LABEL: @test_signextend22
; CHECK-NEXT: ret i256 1

  %res = call i256 @__signextend(i256 255, i256 1)
  ret i256 %res
}

define i256 @test_signextend23() {
; CHECK-LABEL: @test_signextend23
; CHECK-NEXT: ret i256 -49173855447680950519990795082874703144781591387221730505838393986436314155965

  %res = call i256 @__signextend(i256 255, i256 66618233789635244903580189925813204708488393278418833533619190021476815483971)
  ret i256 %res
}

define i256 @test_signextend24() {
; CHECK-LABEL: @test_signextend24
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__signextend(i256 255, i256 -1)
  ret i256 %res
}

define i256 @test_signextend25() {
; CHECK-LABEL: @test_signextend25
; CHECK-NEXT: ret i256 0

  %res = call i256 @__signextend(i256 256, i256 0)
  ret i256 %res
}

define i256 @test_signextend26() {
; CHECK-LABEL: @test_signextend26
; CHECK-NEXT: ret i256 37670211480306196047687443673641227745170897112008692523754794019498533073987

  %res = call i256 @__signextend(i256 256, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_signextend27() {
; CHECK-LABEL: @test_signextend27
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__signextend(i256 256, i256 -1)
  ret i256 %res
}

define i256 @test_signextend28() {
; CHECK-LABEL: @test_signextend28
; CHECK-NEXT: ret i256 1

  %res = call i256 @__signextend(i256 35242523534534534233424343343443, i256 1)
  ret i256 %res
}

define i256 @test_signextend29() {
; CHECK-LABEL: @test_signextend29
; CHECK-NEXT: ret i256 37670211480306196047687443673641227745170897112008692523754794019498533073987

  %res = call i256 @__signextend(i256 35242523534534534233424343343443, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_signextend30() {
; CHECK-LABEL: @test_signextend30
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__signextend(i256 35242523534534534233424343343443, i256 -1)
  ret i256 %res
}

define i256 @test_signextend31() {
; CHECK-LABEL: @test_signextend31
; CHECK-NEXT: ret i256 0

  %res = call i256 @__signextend(i256 -1, i256 0)
  ret i256 %res
}

define i256 @test_signextend32() {
; CHECK-LABEL: @test_signextend32
; CHECK-NEXT: ret i256 1

  %res = call i256 @__signextend(i256 -1, i256 1)
  ret i256 %res
}

define i256 @test_signextend33() {
; CHECK-LABEL: @test_signextend33
; CHECK-NEXT: ret i256 37670211480306196047687443673641227745170897112008692523754794019498533073987

  %res = call i256 @__signextend(i256 -1, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_signextend34() {
; CHECK-LABEL: @test_signextend34
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__signextend(i256 -1, i256 -1)
  ret i256 %res
}

define i256 @test_signextend35() {
; CHECK-LABEL: @test_signextend35
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__signextend(i256 0, i256 33023)
  ret i256 %res
}

attributes #0 = { nounwind readnone willreturn }
