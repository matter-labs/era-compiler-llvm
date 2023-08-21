; RUN: opt -early-cse -S < %s | FileCheck %s

target triple = "syncvm"

declare i256 @__addmod(i256, i256, i256) #0
declare i256 @__mulmod(i256, i256, i256) #0
declare i256 @__signextend(i256, i256) #0
declare i256 @__exp(i256, i256) #0

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

  %res = call i256 @__exp(i256 0, i256 10)
  ret i256 %res
}

define i256 @test_exponent2() {
; CHECK-LABEL: @test_exponent2
; CHECK-NEXT: ret i256 1

  %res = call i256 @__exp(i256 2, i256 undef)
  ret i256 %res
}

define i256 @test_exponent3() {
; CHECK-LABEL: @test_exponent3
; CHECK-NEXT: ret i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968

  %res = call i256 @__exp(i256 2, i256 255)
  ret i256 %res
}

define i256 @test_exponent4() {
; CHECK-LABEL: @test_exponent4
; CHECK-NEXT: ret i256 -26400738010602378953627016196889292963087978848325315750873680393886838386559

  %res = call i256 @__exp(i256 307, i256 32)
  ret i256 %res
}

define i256 @test_exponent5() {
; CHECK-LABEL: @test_exponent5
; CHECK-NEXT: ret i256 0

  %res = call i256 @__exp(i256 undef, i256 2)
  ret i256 %res
}

define i256 @test_exponent6() {
; CHECK-LABEL: @test_exponent6
; CHECK-NEXT: ret i256 1

  %res = call i256 @__exp(i256 0, i256 0)
  ret i256 %res
}

define i256 @test_exponent7() {
; CHECK-LABEL: @test_exponent7
; CHECK-NEXT: ret i256 1

  %res = call i256 @__exp(i256 1, i256 0)
  ret i256 %res
}

define i256 @test_exponent7.1() {
; CHECK-LABEL: @test_exponent7.1
; CHECK-NEXT: ret i256 1

  %res = call i256 @__exp(i256 1, i256 1)
  ret i256 %res
}

define i256 @test_exponent8() {
; CHECK-LABEL: @test_exponent8
; CHECK-NEXT: ret i256 0

  %res = call i256 @__exp(i256 0, i256 433478394034343)
  ret i256 %res
}

define i256 @test_exponent9() {
; CHECK-LABEL: @test_exponent9
; CHECK-NEXT: ret i256 1

  %res = call i256 @__exp(i256 121563127839120, i256 0)
  ret i256 %res
}

define i256 @test_exponent10() {
; CHECK-LABEL: @test_exponent10
; CHECK-NEXT: ret i256 1

  %res = call i256 @__exp(i256 1, i256 433478394034343)
  ret i256 %res
}

define i256 @test_exponent11() {
; CHECK-LABEL: @test_exponent11
; CHECK-NEXT: ret i256 121563127839120

  %res = call i256 @__exp(i256 121563127839120, i256 1)
  ret i256 %res
}

define i256 @test_exponent12() {
; CHECK-LABEL: @test_exponent12
; CHECK-NEXT: ret i256 569381465857367090636427305760163241950353347303833610101782245331441

  %res = call i256 @__exp(i256 21, i256 52)
  ret i256 %res
}

define i256 @test_exponent13() {
; CHECK-LABEL: @test_exponent13
; CHECK-NEXT: ret i256 -680564733841876926926749214863536422911

  ; 0x00000000000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF ^ 2 ->
  ; 0xfffffffffffffffffffffffffffffffe00000000000000000000000000000001
  %res = call i256 @__exp(i256 340282366920938463463374607431768211455, i256 2)
  ret i256 %res
}

define i256 @test_exponent14() {
; CHECK-LABEL: @test_exponent14
; CHECK-NEXT: ret i256 0

  ; 0x0000000000000000000000000000000000000000000000000000000000010000 ^ 16 ->  0
  %res = call i256 @__exp(i256 65536, i256 16)
  ret i256 %res
}

define i256 @test_exponent15() {
; CHECK-LABEL: @test_exponent15
; CHECK-NEXT: ret i256 46756260758475007021788099943083655901358133181480408838873172916982662561791

  ; 0x2fff1ffffffffff5ffffff0fffffffff2ffffffafffafffcffff1ff234ffffff ^
  ; 0xef231ffffffffff4f12fff34ffffffff2fffffbbfffafffdffff22f538ffffff ->
  ; 46756260758475007021788099943083655901358133181480408838873172916982662561791
  %res = call i256 @__exp(i256 21709470740815105492860156599188632070735699051917406219058709325770546741247, i256 108164831314551007501389766781044785045182831393737867957367412557260901056511)
  ret i256 %res
}

define i256 @test_exponent16() {
; CHECK-LABEL: @test_exponent16
; CHECK-NEXT: ret i256 0

  ; 0 ^ 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff -> 0
  %res = call i256 @__exp(i256 0, i256 115792089237316195423570985008687907853269984665640564039457584007913129639935)
  ret i256 %res
}

define i256 @test_exponent17() {
; CHECK-LABEL: @test_exponent17
; CHECK-NEXT: ret i256 1

  ; 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff ^ 0 - > 1
  %res = call i256 @__exp(i256 115792089237316195423570985008687907853269984665640564039457584007913129639935, i256 0)
  ret i256 %res
}

define i256 @test_exponent18() {
; CHECK-LABEL: @test_exponent18
; CHECK-NEXT: ret i256 1

  ; 1 ^ 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff -> 1
  %res = call i256 @__exp(i256 1, i256 115792089237316195423570985008687907853269984665640564039457584007913129639935)
  ret i256 %res
}

define i256 @test_exponent19() {
; CHECK-LABEL: @test_exponent19
; CHECK-NEXT: ret i256 -1

  ; 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff ^ 1 - >
  ; 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
  %res = call i256 @__exp(i256 115792089237316195423570985008687907853269984665640564039457584007913129639935, i256 1)
  ret i256 %res
}

define i256 @test_exponent20() {
; CHECK-LABEL: @test_exponent20
; CHECK-NEXT: ret i256 0

  ; 7437834752357434334343423343443375834785783474 ^
  ; 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff - > 0
  %res = call i256 @__exp(i256 7437834752357434334343423343443375834785783474, i256 115792089237316195423570985008687907853269984665640564039457584007913129639935)
  ret i256 %res
}

define i256 @test_exponent21() {
; CHECK-LABEL: @test_exponent21
; CHECK-NEXT: ret i256 -1

  ; 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff ^
  ; 23784273472384723848213821342323233223 - >
  ; 115792089237316195423570985008687907853269984665640564039457584007913129639935
  %res = call i256 @__exp(i256 115792089237316195423570985008687907853269984665640564039457584007913129639935, i256 23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_exponent22() {
; CHECK-LABEL: @test_exponent22
; CHECK-NEXT: ret i256 -1

  ; 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff ^
  ; 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff ->
  ; 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
  %res = call i256 @__exp(i256 115792089237316195423570985008687907853269984665640564039457584007913129639935, i256 115792089237316195423570985008687907853269984665640564039457584007913129639935)
  ret i256 %res
}

attributes #0 = {nounwind readnone willreturn}
