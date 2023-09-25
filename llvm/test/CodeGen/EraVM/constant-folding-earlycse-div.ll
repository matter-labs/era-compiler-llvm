; RUN: opt -early-cse -S < %s | FileCheck %s

target triple = "eravm"

declare i256 @__div(i256, i256) #0

define i256 @test_div1() {
; CHECK-LABEL: @test_div1
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__div(i256 undef, i256 1)
  ret i256 %res
}

define i256 @test_div2() {
; CHECK-LABEL: @test_div2
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__div(i256 1, i256 undef)
  ret i256 %res
}

define i256 @test_div3() {
; CHECK-LABEL: @test_div3
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 0, i256 0)
  ret i256 %res
}

define i256 @test_div4() {
; CHECK-LABEL: @test_div4
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 0, i256 1)
  ret i256 %res
}

define i256 @test_div5() {
; CHECK-LABEL: @test_div5
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 1, i256 0)
  ret i256 %res
}

define i256 @test_div6() {
; CHECK-LABEL: @test_div6
; CHECK-NEXT: ret i256 1

  %res = call i256 @__div(i256 1, i256 1)
  ret i256 %res
}

define i256 @test_div7() {
; CHECK-LABEL: @test_div7
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 0, i256 433478394034343)
  ret i256 %res
}

define i256 @test_div8() {
; CHECK-LABEL: @test_div8
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 121563127839120, i256 0)
  ret i256 %res
}

define i256 @test_div9() {
; CHECK-LABEL: @test_div9
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 1, i256 433478394034343)
  ret i256 %res
}

define i256 @test_div10() {
; CHECK-LABEL: @test_div10
; CHECK-NEXT: ret i256 121563127839120

  %res = call i256 @__div(i256 121563127839120, i256 1)
  ret i256 %res
}

define i256 @test_div11() {
; CHECK-LABEL: @test_div11
; CHECK-NEXT: ret i256 76

  %res = call i256 @__div(i256 3456789009876545678, i256 45116789098765678)
  ret i256 %res
}

define i256 @test_div12() {
; CHECK-LABEL: @test_div12
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 77345497107738552411838431392522000948863935069627095510731433067785723314337, i256 77345497107738552411838431392522000948863935069627095510731433067785723314338)
  ret i256 %res
}

define i256 @test_div13() {
; CHECK-LABEL: @test_div13
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 456789098765467892304234234234234234234, i256 7863249563247857289401203492047823764573465734573434537783)
  ret i256 %res
}

define i256 @test_div14() {
; CHECK-LABEL: @test_div14
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 0, i256 -1)
  ret i256 %res
}

define i256 @test_div15() {
; CHECK-LABEL: @test_div15
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 -1, i256 0)
  ret i256 %res
}

define i256 @test_div16() {
; CHECK-LABEL: @test_div16
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 1, i256 -1)
  ret i256 %res
}

define i256 @test_div17() {
; CHECK-LABEL: @test_div17
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__div(i256 -1, i256 1)
  ret i256 %res
}

define i256 @test_div18() {
; CHECK-LABEL: @test_div18
; CHECK-NEXT: ret i256 0

  %res = call i256 @__div(i256 7437834752357434334343423343443375834785783474, i256 -1)
  ret i256 %res
}

define i256 @test_div19() {
; CHECK-LABEL: @test_div19
; CHECK-NEXT: ret i256 4868430787753902008116956541571039081557

  %res = call i256 @__div(i256 -1, i256 23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_div20() {
; CHECK-LABEL: @test_div20
; CHECK-NEXT: ret i256 1

  %res = call i256 @__div(i256 -1, i256 -1)
  ret i256 %res
}

attributes #0 = { nounwind readnone willreturn }
