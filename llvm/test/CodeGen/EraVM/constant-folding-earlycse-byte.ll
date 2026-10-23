; RUN: opt -passes=early-cse -S < %s | FileCheck %s

target triple = "eravm"

declare i256 @__byte(i256, i256) #0

define i256 @test_byte1() {
; CHECK-LABEL: @test_byte1
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__byte(i256 undef, i256 1)
  ret i256 %res
}

define i256 @test_byte2() {
; CHECK-LABEL: @test_byte2
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__byte(i256 1, i256 undef)
  ret i256 %res
}

define i256 @test_byte3() {
; CHECK-LABEL: @test_byte3
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 0, i256 0)
  ret i256 %res
}

define i256 @test_byte4() {
; CHECK-LABEL: @test_byte4
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 0, i256 1)
  ret i256 %res
}

define i256 @test_byte5() {
; CHECK-LABEL: @test_byte5
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 0, i256 62786337629547936342664354281295019512044052096983040078175507080572122364)
  ret i256 %res
}

define i256 @test_byte6() {
; CHECK-LABEL: @test_byte6
; CHECK-NEXT: ret i256 83

  %res = call i256 @__byte(i256 0, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_byte7() {
; CHECK-LABEL: @test_byte7
; CHECK-NEXT: ret i256 255

  %res = call i256 @__byte(i256 0, i256 -213508454229078891452382036238048110874681386347114622284045643289719458749)
  ret i256 %res
}

define i256 @test_byte8() {
; CHECK-LABEL: @test_byte8
; CHECK-NEXT: ret i256 255

  %res = call i256 @__byte(i256 0, i256 -1)
  ret i256 %res
}

define i256 @test_byte9() {
; CHECK-LABEL: @test_byte9
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 1, i256 0)
  ret i256 %res
}

define i256 @test_byte10() {
; CHECK-LABEL: @test_byte10
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 1, i256 1)
  ret i256 %res
}

define i256 @test_byte11() {
; CHECK-LABEL: @test_byte11
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 1, i256 15831896390776628077873594548411842773272337831711882241313510853617203623164)
  ret i256 %res
}

define i256 @test_byte12() {
; CHECK-LABEL: @test_byte12
; CHECK-NEXT: ret i256 72

  %res = call i256 @__byte(i256 1, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_byte13() {
; CHECK-LABEL: @test_byte13
; CHECK-NEXT: ret i256 255

  %res = call i256 @__byte(i256 1, i256 -54279028636447639376701285558971354695195688630741054740805195402843884604349)
  ret i256 %res
}

define i256 @test_byte14() {
; CHECK-LABEL: @test_byte14
; CHECK-NEXT: ret i256 255

  %res = call i256 @__byte(i256 1, i256 -1)
  ret i256 %res
}

define i256 @test_byte15() {
; CHECK-LABEL: @test_byte15
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 17, i256 0)
  ret i256 %res
}

define i256 @test_byte16() {
; CHECK-LABEL: @test_byte16
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 17, i256 1)
  ret i256 %res
}

define i256 @test_byte17() {
; CHECK-LABEL: @test_byte17
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 17, i256 16073302433164271703722074696011524995083260326051756269332189763149975074044)
  ret i256 %res
}

define i256 @test_byte18() {
; CHECK-LABEL: @test_byte18
; CHECK-NEXT: ret i256 205

  %res = call i256 @__byte(i256 17, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_byte19() {
; CHECK-LABEL: @test_byte19
; CHECK-NEXT: ret i256 255

  %res = call i256 @__byte(i256 17, i256 -54658164282644196211809801276940316383917872613704572060086920199132892875709)
  ret i256 %res
}

define i256 @test_byte20() {
; CHECK-LABEL: @test_byte20
; CHECK-NEXT: ret i256 255

  %res = call i256 @__byte(i256 17, i256 -1)
  ret i256 %res
}

define i256 @test_byte21() {
; CHECK-LABEL: @test_byte21
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 31, i256 0)
  ret i256 %res
}

define i256 @test_byte22() {
; CHECK-LABEL: @test_byte22
; CHECK-NEXT: ret i256 1

  %res = call i256 @__byte(i256 31, i256 1)
  ret i256 %res
}

define i256 @test_byte23() {
; CHECK-LABEL: @test_byte23
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 31, i256 16073302433164271703722074696011524995083277336827658260012929812626463325184)
  ret i256 %res
}

define i256 @test_byte24() {
; CHECK-LABEL: @test_byte24
; CHECK-NEXT: ret i256 67

  %res = call i256 @__byte(i256 31, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_byte25() {
; CHECK-LABEL: @test_byte25
; CHECK-NEXT: ret i256 255

  %res = call i256 @__byte(i256 31, i256 -54658164282644196211809801276940316383918434904861343304715684682168181439489)
  ret i256 %res
}

define i256 @test_byte26() {
; CHECK-LABEL: @test_byte26
; CHECK-NEXT: ret i256 255

  %res = call i256 @__byte(i256 31, i256 -1)
  ret i256 %res
}

define i256 @test_byte27() {
; CHECK-LABEL: @test_byte27
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 32, i256 0)
  ret i256 %res
}

define i256 @test_byte28() {
; CHECK-LABEL: @test_byte28
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 32, i256 1)
  ret i256 %res
}

define i256 @test_byte29() {
; CHECK-LABEL: @test_byte29
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 32, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_byte30() {
; CHECK-LABEL: @test_byte30
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 32, i256 -1)
  ret i256 %res
}

define i256 @test_byte31() {
; CHECK-LABEL: @test_byte31
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 35242523534534534233424343343443, i256 0)
  ret i256 %res
}

define i256 @test_byte32() {
; CHECK-LABEL: @test_byte32
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 35242523534534534233424343343443, i256 1)
  ret i256 %res
}

define i256 @test_byte33() {
; CHECK-LABEL: @test_byte33
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 35242523534534534233424343343443, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_byte34() {
; CHECK-LABEL: @test_byte34
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 35242523534534534233424343343443, i256 -1)
  ret i256 %res
}

define i256 @test_byte35() {
; CHECK-LABEL: @test_byte35
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 -1, i256 0)
  ret i256 %res
}

define i256 @test_byte36() {
; CHECK-LABEL: @test_byte36
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 -1, i256 1)
  ret i256 %res
}

define i256 @test_byte37() {
; CHECK-LABEL: @test_byte37
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 -1, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_byte38() {
; CHECK-LABEL: @test_byte38
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 -1, i256 -1)
  ret i256 %res
}

define i256 @test_byte39() {
; CHECK-LABEL: @test_byte39
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 14474011154664524427946373126085988481658748083205070504932198000989141204992, i256 455867356320691211509944977504407603390036387149619137164185182714736811808)
  ret i256 %res
}

define i256 @test_byte40() {
; CHECK-LABEL: @test_byte40
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 14474011154664524427946373126085988481658748083205070504932198000989141204993, i256 455867356320691211509944977504407603390036387149619137164185182714736811808)
  ret i256 %res
}

define i256 @test_byte41() {
; CHECK-LABEL: @test_byte41
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 14474011154664524427946373126085988481658748083205070504932198000989141205023, i256 455867356320691211509944977504407603390036387149619137164185182714736811808)
  ret i256 %res
}

define i256 @test_byte42() {
; CHECK-LABEL: @test_byte42
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 28948022309329048855892746252171976963317496166410141009864396001978282409984, i256 455867356320691211509944977504407603390036387149619137164185182714736811808)
  ret i256 %res
}

define i256 @test_byte43() {
; CHECK-LABEL: @test_byte43
; CHECK-NEXT: ret i256 0

  %res = call i256 @__byte(i256 43422033463993573283839119378257965444976244249615211514796594002967423614976, i256 455867356320691211509944977504407603390036387149619137164185182714736811808)
  ret i256 %res
}

attributes #0 = { nounwind readnone willreturn }
