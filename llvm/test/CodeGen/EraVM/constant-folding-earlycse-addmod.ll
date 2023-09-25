; RUN: opt -early-cse -S < %s | FileCheck %s

target triple = "eravm"

declare i256 @__addmod(i256, i256, i256) #0

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
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__addmod(i256 48, i256 undef, i256 5)
  ret i256 %res
}

define i256 @test_addmod4() {
; CHECK-LABEL: @test_addmod4
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__addmod(i256 undef, i256 48, i256 5)
  ret i256 %res
}

define i256 @test_addmod5() {
; CHECK-LABEL: @test_addmod5
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__addmod(i256 48, i256 1, i256 undef)
  ret i256 %res
}

attributes #0 = { nounwind readnone willreturn }
