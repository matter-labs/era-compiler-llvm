; RUN: opt -early-cse -S < %s | FileCheck %s

target triple = "syncvm"

declare i256 @__addmod(i256, i256, i256) #0
declare i256 @__mulmod(i256, i256, i256) #0
declare i256 @__signextend(i256, i256) #0
declare i256 @__exp(i256, i256) #0
declare i256 @__div(i256, i256) #0
declare i256 @__sdiv(i256, i256) #0
declare i256 @__mod(i256, i256) #0
declare i256 @__smod(i256, i256) #0
declare i256 @__shl(i256, i256) #0
declare i256 @__shr(i256, i256) #0
declare i256 @__sar(i256, i256) #0
declare i256 @__byte(i256, i256) #0

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

define i256 @test_exponent1() {
; CHECK-LABEL: @test_exponent1
; CHECK-NEXT: ret i256 0

  %res = call i256 @__exp(i256 0, i256 10)
  ret i256 %res
}

define i256 @test_exponent2() {
; CHECK-LABEL: @test_exponent2
; CHECK-NEXT: ret i256 poison

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
; CHECK-NEXT: ret i256 poison

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

define i256 @test_sdiv1() {
; CHECK-LABEL: @test_sdiv1
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__sdiv(i256 undef, i256 1)
 ret i256 %res
}

define i256 @test_sdiv2() {
; CHECK-LABEL: @test_sdiv2
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__sdiv(i256 1, i256 undef)
  ret i256 %res
}

define i256 @test_sdiv3() {
; CHECK-LABEL: @test_sdiv3
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 0, i256 0)
  ret i256 %res
}

define i256 @test_sdiv4() {
; CHECK-LABEL: @test_sdiv4
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 0, i256 1)
  ret i256 %res
}

define i256 @test_sdiv5() {
; CHECK-LABEL: @test_sdiv5
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 1, i256 0)
  ret i256 %res
}

define i256 @test_sdiv6() {
; CHECK-LABEL: @test_sdiv6
; CHECK-NEXT: ret i256 1

  %res = call i256 @__sdiv(i256 1, i256 1)
  ret i256 %res
}

define i256 @test_sdiv7() {
; CHECK-LABEL: @test_sdiv7
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 0, i256 433478394034343)
  ret i256 %res
}

define i256 @test_sdiv8() {
; CHECK-LABEL: @test_sdiv8
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 121563127839120, i256 0)
  ret i256 %res
}

define i256 @test_sdiv9() {
; CHECK-LABEL: @test_sdiv9
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 1, i256 433478394034343)
  ret i256 %res
}

define i256 @test_sdiv10() {
; CHECK-LABEL: @test_sdiv10
; CHECK-NEXT: ret i256 121563127839120

  %res = call i256 @__sdiv(i256 121563127839120, i256 1)
  ret i256 %res
}

define i256 @test_sdiv11() {
; CHECK-LABEL: @test_sdiv11
; CHECK-NEXT: ret i256 76

  %res = call i256 @__sdiv(i256 3456789009876545678, i256 45116789098765678)
  ret i256 %res
}

define i256 @test_sdiv12() {
; CHECK-LABEL: @test_sdiv12
; CHECK-NEXT: ret i256 1

  %res = call i256 @__sdiv(i256 6873546235472354672354762358492378590345034805934590348534, i256 6873546235472354672354762358492378590345034805934590348534)
  ret i256 %res
}

define i256 @test_sdiv13() {
; CHECK-LABEL: @test_sdiv13
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 4975441334415930272106565762092058540570194653601742986070443062840017289377, i256 4975441334415930272106565762092058540570194653601742986070443062840017289378)
  ret i256 %res
}

define i256 @test_sdiv14() {
; CHECK-LABEL: @test_sdiv14
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 456789098765467892304234234234234234234, i256 7863249563247857289401203492047823764573465734573434537783)
  ret i256 %res
}

define i256 @test_sdiv15() {
; CHECK-LABEL: @test_sdiv15
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 0, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sdiv16() {
; CHECK-LABEL: @test_sdiv16
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 0)
  ret i256 %res
}

define i256 @test_sdiv17() {
; CHECK-LABEL: @test_sdiv17
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 1, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sdiv18() {
; CHECK-LABEL: @test_sdiv18
; CHECK-NEXT: ret i256 57896044618658097711785492504343953926634992332820282019728792003956564819967

  %res = call i256 @__sdiv(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 1)
  ret i256 %res
}

define i256 @test_sdiv19() {
; CHECK-LABEL: @test_sdiv19
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 7437834752357434334343423343443375834785783474, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sdiv20() {
; CHECK-LABEL: @test_sdiv20
; CHECK-NEXT: ret i256 2434215393876951004058478270785519540778

  %res = call i256 @__sdiv(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_sdiv21() {
; CHECK-LABEL: @test_sdiv21
; CHECK-NEXT: ret i256 1

  %res = call i256 @__sdiv(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sdiv22() {
; CHECK-LABEL: @test_sdiv22
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 0, i256 -1)
  ret i256 %res
}

define i256 @test_sdiv23() {
; CHECK-LABEL: @test_sdiv23
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -1, i256 0)
  ret i256 %res
}

define i256 @test_sdiv24() {
; CHECK-LABEL: @test_sdiv24
; CHECK-NEXT: ret i256 1

  %res = call i256 @__sdiv(i256 -1, i256 -1)
  ret i256 %res
}

define i256 @test_sdiv25() {
; CHECK-LABEL: @test_sdiv25
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 0, i256 -433478394034343)
  ret i256 %res
}

define i256 @test_sdiv26() {
; CHECK-LABEL: @test_sdiv26
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -121563127839120, i256 0)
  ret i256 %res
}

define i256 @test_sdiv27() {
; CHECK-LABEL: @test_sdiv27
; CHECK-NEXT: ret i256 121563127839120

  %res = call i256 @__sdiv(i256 -121563127839120, i256 -1)
  ret i256 %res
}

define i256 @test_sdiv28() {
; CHECK-LABEL: @test_sdiv28
; CHECK-NEXT: ret i256 76

  %res = call i256 @__sdiv(i256 -3456789009876545678, i256 -45116789098765678)
  ret i256 %res
}

define i256 @test_sdiv29() {
; CHECK-LABEL: @test_sdiv29
; CHECK-NEXT: ret i256 1

  %res = call i256 @__sdiv(i256 -6873546235472354672354762358492378590345034805934590348534, i256 -6873546235472354672354762358492378590345034805934590348534)
  ret i256 %res
}

define i256 @test_sdiv30() {
; CHECK-LABEL: @test_sdiv30
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -4975441334415930272106565762092058540570194653601742986070443062840017289377, i256 -4975441334415930272106565762092058540570194653601742986070443062840017289378)
  ret i256 %res
}

define i256 @test_sdiv31() {
; CHECK-LABEL: @test_sdiv31
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -456789098765467892304234234234234234234, i256 -7863249563247857289401203492047823764573465734573434537783)
  ret i256 %res
}

define i256 @test_sdiv32() {
; CHECK-LABEL: @test_sdiv32
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 0, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sdiv33() {
; CHECK-LABEL: @test_sdiv33
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 0)
  ret i256 %res
}

define i256 @test_sdiv34() {
; CHECK-LABEL: @test_sdiv34
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -1, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sdiv35() {
; CHECK-LABEL: @test_sdiv35
; CHECK-NEXT: ret i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968

  %res = call i256 @__sdiv(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 -1)
  ret i256 %res
}

define i256 @test_sdiv36() {
; CHECK-LABEL: @test_sdiv36
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -7437834752357434334343423343443375834785783474, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sdiv37() {
; CHECK-LABEL: @test_sdiv37
; CHECK-NEXT: ret i256 2434215393876951004058478270785519540778

  %res = call i256 @__sdiv(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 -23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_sdiv38() {
; CHECK-LABEL: @test_sdiv38
; CHECK-NEXT: ret i256 1

  %res = call i256 @__sdiv(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sdiv39() {
; CHECK-LABEL: @test_sdiv39
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sdiv(i256 1, i256 -1)
  ret i256 %res
}

define i256 @test_sdiv40() {
; CHECK-LABEL: @test_sdiv40
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sdiv(i256 -1, i256 1)
  ret i256 %res
}

define i256 @test_sdiv41() {
; CHECK-LABEL: @test_sdiv41
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 1, i256 -433478394034343)
  ret i256 %res
}

define i256 @test_sdiv42() {
; CHECK-LABEL: @test_sdiv42
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -1, i256 433478394034343)
  ret i256 %res
}

define i256 @test_sdiv43() {
; CHECK-LABEL: @test_sdiv43
; CHECK-NEXT: ret i256 -121563127839120

  %res = call i256 @__sdiv(i256 121563127839120, i256 -1)
  ret i256 %res
}

define i256 @test_sdiv44() {
; CHECK-LABEL: @test_sdiv44
; CHECK-NEXT: ret i256 -121563127839120

  %res = call i256 @__sdiv(i256 -121563127839120, i256 1)
  ret i256 %res
}

define i256 @test_sdiv45() {
; CHECK-LABEL: @test_sdiv45
; CHECK-NEXT: ret i256 -76

  %res = call i256 @__sdiv(i256 3456789009876545678, i256 -45116789098765678)
  ret i256 %res
}

define i256 @test_sdiv46() {
; CHECK-LABEL: @test_sdiv46
; CHECK-NEXT: ret i256 -76

  %res = call i256 @__sdiv(i256 -3456789009876545678, i256 45116789098765678)
  ret i256 %res
}

define i256 @test_sdiv47() {
; CHECK-LABEL: @test_sdiv47
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sdiv(i256 6873546235472354672354762358492378590345034805934590348534, i256 -6873546235472354672354762358492378590345034805934590348534)
  ret i256 %res
}

define i256 @test_sdiv48() {
; CHECK-LABEL: @test_sdiv48
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 4975441334415930272106565762092058540570194653601742986070443062840017289377, i256 -4975441334415930272106565762092058540570194653601742986070443062840017289378)
  ret i256 %res
}

define i256 @test_sdiv49() {
; CHECK-LABEL: @test_sdiv49
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -4975441334415930272106565762092058540570194653601742986070443062840017289377, i256 4975441334415930272106565762092058540570194653601742986070443062840017289378)
  ret i256 %res
}

define i256 @test_sdiv50() {
; CHECK-LABEL: @test_sdiv50
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 456789098765467892304234234234234234234, i256 -7863249563247857289401203492047823764573465734573434537783)
  ret i256 %res
}

define i256 @test_sdiv51() {
; CHECK-LABEL: @test_sdiv51
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -456789098765467892304234234234234234234, i256 7863249563247857289401203492047823764573465734573434537783)
  ret i256 %res
}

define i256 @test_sdiv52() {
; CHECK-LABEL: @test_sdiv52
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 1, i256 57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sdiv53() {
; CHECK-LABEL: @test_sdiv53
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -1, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sdiv54() {
; CHECK-LABEL: @test_sdiv54
; CHECK-NEXT: ret i256 -57896044618658097711785492504343953926634992332820282019728792003956564819967

  %res = call i256 @__sdiv(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 -1)
  ret i256 %res
}

define i256 @test_sdiv55() {
; CHECK-LABEL: @test_sdiv55
; CHECK-NEXT: ret i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968

  %res = call i256 @__sdiv(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 1)
  ret i256 %res
}

define i256 @test_sdiv56() {
; CHECK-LABEL: @test_sdiv56
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 7437834752357434334343423343443375834785783474, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sdiv57() {
; CHECK-LABEL: @test_sdiv57
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 -7437834752357434334343423343443375834785783474, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sdiv58() {
; CHECK-LABEL: @test_sdiv58
; CHECK-NEXT: ret i256 -2434215393876951004058478270785519540778

  %res = call i256 @__sdiv(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_sdiv59() {
; CHECK-LABEL: @test_sdiv59
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sdiv(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sdiv60() {
; CHECK-LABEL: @test_sdiv60
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sdiv(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sdiv61() {
; CHECK-LABEL: @test_sdiv61
; CHECK-NEXT: ret i256 -2434215393876951004058478270785519540778

  %res = call i256 @__sdiv(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 -23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_mod1() {
; CHECK-LABEL: @test_mod1
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__mod(i256 undef, i256 1)
  ret i256 %res
}

define i256 @test_mod2() {
; CHECK-LABEL: @test_mod2
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__mod(i256 1, i256 undef)
  ret i256 %res
}

define i256 @test_mod3() {
; CHECK-LABEL: @test_mod3
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 0, i256 0)
  ret i256 %res
}

define i256 @test_mod4() {
; CHECK-LABEL: @test_mod4
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 0, i256 1)
  ret i256 %res
}

define i256 @test_mod5() {
; CHECK-LABEL: @test_mod5
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 1, i256 0)
  ret i256 %res
}

define i256 @test_mod6() {
; CHECK-LABEL: @test_mod6
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 1, i256 1)
  ret i256 %res
}

define i256 @test_mod7() {
; CHECK-LABEL: @test_mod7
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 0, i256 433478394034343)
  ret i256 %res
}

define i256 @test_mod8() {
; CHECK-LABEL: @test_mod8
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 121563127839120, i256 0)
  ret i256 %res
}

define i256 @test_mod9() {
; CHECK-LABEL: @test_mod9
; CHECK-NEXT: ret i256 1

  %res = call i256 @__mod(i256 1, i256 433478394034343)
  ret i256 %res
}

define i256 @test_mod10() {
; CHECK-LABEL: @test_mod10
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 121563127839120, i256 1)
  ret i256 %res
}

define i256 @test_mod11() {
; CHECK-LABEL: @test_mod11
; CHECK-NEXT: ret i256 27913038370354150

  %res = call i256 @__mod(i256 3456789009876545678, i256 45116789098765678)
  ret i256 %res
}

define i256 @test_mod12() {
; CHECK-LABEL: @test_mod12
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 15903955902367335447395448984, i256 67897657890876)
  ret i256 %res
}

define i256 @test_mod13() {
; CHECK-LABEL: @test_mod13
; CHECK-NEXT: ret i256 1

  %res = call i256 @__mod(i256 77345497107738552411838431392522000948863935069627095510731433067785723314338, i256 77345497107738552411838431392522000948863935069627095510731433067785723314337)
  ret i256 %res
}

define i256 @test_mod14() {
; CHECK-LABEL: @test_mod14
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 6873546235472354672354762358492378590345034805934590348534, i256 6873546235472354672354762358492378590345034805934590348534)
  ret i256 %res
}

define i256 @test_mod15() {
; CHECK-LABEL: @test_mod15
; CHECK-NEXT: ret i256 -38446592129577643011732553616165906904406049596013468528726150940127406325599

  %res = call i256 @__mod(i256 77345497107738552411838431392522000948863935069627095510731433067785723314337, i256 77345497107738552411838431392522000948863935069627095510731433067785723314338)
  ret i256 %res
}

define i256 @test_mod16() {
; CHECK-LABEL: @test_mod16
; CHECK-NEXT: ret i256 456789098765467892304234234234234234234

  %res = call i256 @__mod(i256 456789098765467892304234234234234234234, i256 7863249563247857289401203492047823764573465734573434537783)
  ret i256 %res
}

define i256 @test_mod17() {
; CHECK-LABEL: @test_mod17
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 0, i256 115792089237316195423570985008687907853269984665640564039457584007913129639935)
  ret i256 %res
}

define i256 @test_mod18() {
; CHECK-LABEL: @test_mod18
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 115792089237316195423570985008687907853269984665640564039457584007913129639935, i256 0)
  ret i256 %res
}

define i256 @test_mod19() {
; CHECK-LABEL: @test_mod19
; CHECK-NEXT: ret i256 1

  %res = call i256 @__mod(i256 1, i256 115792089237316195423570985008687907853269984665640564039457584007913129639935)
  ret i256 %res
}

define i256 @test_mod20() {
; CHECK-LABEL: @test_mod20
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 115792089237316195423570985008687907853269984665640564039457584007913129639935, i256 1)
  ret i256 %res
}

define i256 @test_mod21() {
; CHECK-LABEL: @test_mod21
; CHECK-NEXT: ret i256 7437834752357434334343423343443375834785783474

  %res = call i256 @__mod(i256 7437834752357434334343423343443375834785783474, i256 115792089237316195423570985008687907853269984665640564039457584007913129639935)
  ret i256 %res
}

define i256 @test_mod22() {
; CHECK-LABEL: @test_mod22
; CHECK-NEXT: ret i256 6417771337354779485678372628500671724

  %res = call i256 @__mod(i256 115792089237316195423570985008687907853269984665640564039457584007913129639935, i256 23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_mod23() {
; CHECK-LABEL: @test_mod23
; CHECK-NEXT: ret i256 0

  %res = call i256 @__mod(i256 115792089237316195423570985008687907853269984665640564039457584007913129639935, i256 115792089237316195423570985008687907853269984665640564039457584007913129639935)
  ret i256 %res
}

define i256 @test_smod1() {
; CHECK-LABEL: @test_smod1
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__smod(i256 undef, i256 1)
  ret i256 %res
}

define i256 @test_smod2() {
; CHECK-LABEL: @test_smod2
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__smod(i256 1, i256 undef)
  ret i256 %res
}

define i256 @test_smod3() {
; CHECK-LABEL: @test_smod3
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 0, i256 0)
  ret i256 %res
}

define i256 @test_smod4() {
; CHECK-LABEL: @test_smod4
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 0, i256 1)
  ret i256 %res
}

define i256 @test_smod5() {
; CHECK-LABEL: @test_smod5
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 1, i256 0)
  ret i256 %res
}

define i256 @test_smod6() {
; CHECK-LABEL: @test_smod6
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 1, i256 1)
  ret i256 %res
}

define i256 @test_smod7() {
; CHECK-LABEL: @test_smod7
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 0, i256 433478394034343)
  ret i256 %res
}

define i256 @test_smod8() {
; CHECK-LABEL: @test_smod8
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 121563127839120, i256 0)
  ret i256 %res
}

define i256 @test_smod9() {
; CHECK-LABEL: @test_smod9
; CHECK-NEXT: ret i256 1

  %res = call i256 @__smod(i256 1, i256 433478394034343)
  ret i256 %res
}

define i256 @test_smod10() {
; CHECK-LABEL: @test_smod10
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 121563127839120, i256 1)
  ret i256 %res
}

define i256 @test_smod11() {
; CHECK-LABEL: @test_smod11
; CHECK-NEXT: ret i256 27913038370354150

  %res = call i256 @__smod(i256 3456789009876545678, i256 45116789098765678)
  ret i256 %res
}

define i256 @test_smod12() {
; CHECK-LABEL: @test_smod12
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 15903955902367335447395448984, i256 67897657890876)
  ret i256 %res
}

define i256 @test_smod13() {
; CHECK-LABEL: @test_smod13
; CHECK-NEXT: ret i256 1

  %res = call i256 @__smod(i256 4975441334415930272106565762092058540570194653601742986070443062840017289378, i256 4975441334415930272106565762092058540570194653601742986070443062840017289377)
  ret i256 %res
}

define i256 @test_smod14() {
; CHECK-LABEL: @test_smod14
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 6873546235472354672354762358492378590345034805934590348534, i256 6873546235472354672354762358492378590345034805934590348534)
  ret i256 %res
}

define i256 @test_smod15() {
; CHECK-LABEL: @test_smod15
; CHECK-NEXT: ret i256 4975441334415930272106565762092058540570194653601742986070443062840017289377

  %res = call i256 @__smod(i256 4975441334415930272106565762092058540570194653601742986070443062840017289377, i256 4975441334415930272106565762092058540570194653601742986070443062840017289378)
  ret i256 %res
}

define i256 @test_smod16() {
; CHECK-LABEL: @test_smod16
; CHECK-NEXT: ret i256 456789098765467892304234234234234234234

  %res = call i256 @__smod(i256 456789098765467892304234234234234234234, i256 7863249563247857289401203492047823764573465734573434537783)
  ret i256 %res
}

define i256 @test_smod17() {
; CHECK-LABEL: @test_smod17
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 0, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_smod18() {
; CHECK-LABEL: @test_smod18
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 0)
  ret i256 %res
}

define i256 @test_smod19() {
; CHECK-LABEL: @test_smod19
; CHECK-NEXT: ret i256 1

  %res = call i256 @__smod(i256 1, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_smod20() {
; CHECK-LABEL: @test_smod20
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 1)
  ret i256 %res
}

define i256 @test_smod21() {
; CHECK-LABEL: @test_smod21
; CHECK-NEXT: ret i256 7437834752357434334343423343443375834785783474

  %res = call i256 @__smod(i256 7437834752357434334343423343443375834785783474, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_smod22() {
; CHECK-LABEL: @test_smod22
; CHECK-NEXT: ret i256 15101022404869751666946096985411952473

  %res = call i256 @__smod(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_smod23() {
; CHECK-LABEL: @test_smod23
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_smod24() {
; CHECK-LABEL: @test_smod24
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 0, i256 -1)
  ret i256 %res
}

define i256 @test_smod25() {
; CHECK-LABEL: @test_smod25
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -1, i256 0)
  ret i256 %res
}

define i256 @test_smod26() {
; CHECK-LABEL: @test_smod26
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -1, i256 -1)
  ret i256 %res
}

define i256 @test_smod27() {
; CHECK-LABEL: @test_smod27
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 0, i256 -433478394034343)
  ret i256 %res
}

define i256 @test_smod28() {
; CHECK-LABEL: @test_smod28
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -121563127839120, i256 0)
  ret i256 %res
}

define i256 @test_smod29() {
; CHECK-LABEL: @test_smod29
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__smod(i256 -1, i256 -433478394034343)
  ret i256 %res
}

define i256 @test_smod30() {
; CHECK-LABEL: @test_smod30
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -121563127839120, i256 -1)
  ret i256 %res
}

define i256 @test_smod31() {
; CHECK-LABEL: @test_smod31
; CHECK-NEXT: ret i256 -27913038370354150

  %res = call i256 @__smod(i256 -3456789009876545678, i256 -45116789098765678)
  ret i256 %res
}

define i256 @test_smod32() {
; CHECK-LABEL: @test_smod32
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -15903955902367335447395448984, i256 -67897657890876)
  ret i256 %res
}

define i256 @test_smod33() {
; CHECK-LABEL: @test_smod33
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__smod(i256 -4975441334415930272106565762092058540570194653601742986070443062840017289378, i256 -4975441334415930272106565762092058540570194653601742986070443062840017289377)
  ret i256 %res
}

define i256 @test_smod34() {
; CHECK-LABEL: @test_smod34
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -6873546235472354672354762358492378590345034805934590348534, i256 -6873546235472354672354762358492378590345034805934590348534)
  ret i256 %res
}

define i256 @test_smod35() {
; CHECK-LABEL: @test_smod35
; CHECK-NEXT: ret i256 -4975441334415930272106565762092058540570194653601742986070443062840017289377

  %res = call i256 @__smod(i256 -4975441334415930272106565762092058540570194653601742986070443062840017289377, i256 -4975441334415930272106565762092058540570194653601742986070443062840017289378)
  ret i256 %res
}

define i256 @test_smod36() {
; CHECK-LABEL: @test_smod36
; CHECK-NEXT: ret i256 -456789098765467892304234234234234234234

  %res = call i256 @__smod(i256 -456789098765467892304234234234234234234, i256 -7863249563247857289401203492047823764573465734573434537783)
  ret i256 %res
}

define i256 @test_smod37() {
; CHECK-LABEL: @test_smod37
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 0, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_smod38() {
; CHECK-LABEL: @test_smod38
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 0)
  ret i256 %res
}

define i256 @test_smod39() {
; CHECK-LABEL: @test_smod39
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__smod(i256 -1, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_smod40() {
; CHECK-LABEL: @test_smod40
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 -1)
  ret i256 %res
}

define i256 @test_smod40.1() {
; CHECK-LABEL: @test_smod40.1
; CHECK-NEXT: ret i256 -7437834752357434334343423343443375834785783474

  %res = call i256 @__smod(i256 -7437834752357434334343423343443375834785783474, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_smod41() {
; CHECK-LABEL: @test_smod41
; CHECK-NEXT: ret i256 -15101022404869751666946096985411952474

  %res = call i256 @__smod(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 -23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_smod42() {
; CHECK-LABEL: @test_smod42
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_smod43() {
; CHECK-LABEL: @test_smod43
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 1, i256 -1)
  ret i256 %res
}

define i256 @test_smod44() {
; CHECK-LABEL: @test_smod44
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -1, i256 1)
  ret i256 %res
}

define i256 @test_smod45() {
; CHECK-LABEL: @test_smod45
; CHECK-NEXT: ret i256 1

  %res = call i256 @__smod(i256 1, i256 -433478394034343)
  ret i256 %res
}

define i256 @test_smod46() {
; CHECK-LABEL: @test_smod46
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__smod(i256 -1, i256 433478394034343)
  ret i256 %res
}

define i256 @test_smod47() {
; CHECK-LABEL: @test_smod47
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 121563127839120, i256 -1)
  ret i256 %res
}

define i256 @test_smod48() {
; CHECK-LABEL: @test_smod48
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -121563127839120, i256 1)
  ret i256 %res
}

define i256 @test_smod49() {
; CHECK-LABEL: @test_smod49
; CHECK-NEXT: ret i256 27913038370354150

  %res = call i256 @__smod(i256 3456789009876545678, i256 -45116789098765678)
  ret i256 %res
}

define i256 @test_smod50() {
; CHECK-LABEL: @test_smod50
; CHECK-NEXT: ret i256 -27913038370354150

  %res = call i256 @__smod(i256 -3456789009876545678, i256 45116789098765678)
  ret i256 %res
}

define i256 @test_smod51() {
; CHECK-LABEL: @test_smod51
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 15903955902367335447395448984, i256 -67897657890876)
  ret i256 %res
}

define i256 @test_smod52() {
; CHECK-LABEL: @test_smod52
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -15903955902367335447395448984, i256 67897657890876)
  ret i256 %res
}

define i256 @test_smod53() {
; CHECK-LABEL: @test_smod53
; CHECK-NEXT: ret i256 1

  %res = call i256 @__smod(i256 4975441334415930272106565762092058540570194653601742986070443062840017289378, i256 -4975441334415930272106565762092058540570194653601742986070443062840017289377)
  ret i256 %res
}

define i256 @test_smod54() {
; CHECK-LABEL: @test_smod54
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__smod(i256 -4975441334415930272106565762092058540570194653601742986070443062840017289378, i256 4975441334415930272106565762092058540570194653601742986070443062840017289377)
  ret i256 %res
}

define i256 @test_smod55() {
; CHECK-LABEL: @test_smod55
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 6873546235472354672354762358492378590345034805934590348534, i256 -6873546235472354672354762358492378590345034805934590348534)
  ret i256 %res
}

define i256 @test_smod56() {
; CHECK-LABEL: @test_smod56
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -6873546235472354672354762358492378590345034805934590348534, i256 6873546235472354672354762358492378590345034805934590348534)
  ret i256 %res
}

define i256 @test_smod57() {
; CHECK-LABEL: @test_smod57
; CHECK-NEXT: ret i256 4975441334415930272106565762092058540570194653601742986070443062840017289377

  %res = call i256 @__smod(i256 4975441334415930272106565762092058540570194653601742986070443062840017289377, i256 -4975441334415930272106565762092058540570194653601742986070443062840017289378)
  ret i256 %res
}

define i256 @test_smod58() {
; CHECK-LABEL: @test_smod58
; CHECK-NEXT: ret i256 -4975441334415930272106565762092058540570194653601742986070443062840017289377

  %res = call i256 @__smod(i256 -4975441334415930272106565762092058540570194653601742986070443062840017289377, i256 4975441334415930272106565762092058540570194653601742986070443062840017289378)
  ret i256 %res
}

define i256 @test_smod59() {
; CHECK-LABEL: @test_smod59
; CHECK-NEXT: ret i256 456789098765467892304234234234234234234

  %res = call i256 @__smod(i256 456789098765467892304234234234234234234, i256 -7863249563247857289401203492047823764573465734573434537783)
  ret i256 %res
}

define i256 @test_smod60() {
; CHECK-LABEL: @test_smod60
; CHECK-NEXT: ret i256 -456789098765467892304234234234234234234

  %res = call i256 @__smod(i256 -456789098765467892304234234234234234234, i256 7863249563247857289401203492047823764573465734573434537783)
  ret i256 %res
}

define i256 @test_smod61() {
; CHECK-LABEL: @test_smod61
; CHECK-NEXT: ret i256 1

  %res = call i256 @__smod(i256 1, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_smod62() {
; CHECK-LABEL: @test_smod62
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__smod(i256 -1, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_smod63() {
; CHECK-LABEL: @test_smod63
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 -1)
  ret i256 %res
}

define i256 @test_smod64() {
; CHECK-LABEL: @test_smod64
; CHECK-NEXT: ret i256 0

  %res = call i256 @__smod(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 1)
  ret i256 %res
}

define i256 @test_smod65() {
; CHECK-LABEL: @test_smod65
; CHECK-NEXT: ret i256 7437834752357434334343423343443375834785783474

  %res = call i256 @__smod(i256 7437834752357434334343423343443375834785783474, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_smod66() {
; CHECK-LABEL: @test_smod66
; CHECK-NEXT: ret i256 -7437834752357434334343423343443375834785783474

  %res = call i256 @__smod(i256 -7437834752357434334343423343443375834785783474, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_smod67() {
; CHECK-LABEL: @test_smod67
; CHECK-NEXT: ret i256 15101022404869751666946096985411952473

  %res = call i256 @__smod(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 -23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_smod68() {
; CHECK-LABEL: @test_smod68
; CHECK-NEXT: ret i256 -15101022404869751666946096985411952474

  %res = call i256 @__smod(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 23784273472384723848213821342323233223)
  ret i256 %res
}

define i256 @test_smod69() {
; CHECK-LABEL: @test_smod69
; CHECK-NEXT: ret i256 57896044618658097711785492504343953926634992332820282019728792003956564819967

  %res = call i256 @__smod(i256 57896044618658097711785492504343953926634992332820282019728792003956564819967, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_smod70() {
; CHECK-LABEL: @test_smod70
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__smod(i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

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

define i256 @test_sar1() {
; CHECK-LABEL: @test_sar1
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__sar(i256 undef, i256 1)
  ret i256 %res
}

define i256 @test_sar2() {
; CHECK-LABEL: @test_sar2
; CHECK-NEXT: ret i256 poison

  %res = call i256 @__sar(i256 1, i256 undef)
  ret i256 %res
}

define i256 @test_sar3() {
; CHECK-LABEL: @test_sar3
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 0, i256 0)
  ret i256 %res
}

define i256 @test_sar4() {
; CHECK-LABEL: @test_sar4
; CHECK-NEXT: ret i256 1

  %res = call i256 @__sar(i256 0, i256 1)
  ret i256 %res
}

define i256 @test_sar5() {
; CHECK-LABEL: @test_sar5
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 0, i256 -1)
  ret i256 %res
}

define i256 @test_sar6() {
; CHECK-LABEL: @test_sar6
; CHECK-NEXT: ret i256 37670211480306196047687443673641227745170897112008692523754794019498533073987

  %res = call i256 @__sar(i256 0, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_sar7() {
; CHECK-LABEL: @test_sar7
; CHECK-NEXT: ret i256 -632574534856236475345634624374238423192181237123712631236123123

  %res = call i256 @__sar(i256 0, i256 -632574534856236475345634624374238423192181237123712631236123123)
  ret i256 %res
}

define i256 @test_sar8() {
; CHECK-LABEL: @test_sar8
; CHECK-NEXT: ret i256 57896044618658097711785492504343953926634992332820282019728792003956564819967

  %res = call i256 @__sar(i256 0, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sar9() {
; CHECK-LABEL: @test_sar9
; CHECK-NEXT: ret i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968

  %res = call i256 @__sar(i256 0, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sar10() {
; CHECK-LABEL: @test_sar10
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 1, i256 0)
  ret i256 %res
}

define i256 @test_sar11() {
; CHECK-LABEL: @test_sar11
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 1, i256 1)
  ret i256 %res
}

define i256 @test_sar12() {
; CHECK-LABEL: @test_sar12
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 1, i256 -1)
  ret i256 %res
}

define i256 @test_sar13() {
; CHECK-LABEL: @test_sar13
; CHECK-NEXT: ret i256 18835105740153098023843721836820613872585448556004346261877397009749266536993

  %res = call i256 @__sar(i256 1, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_sar14() {
; CHECK-LABEL: @test_sar14
; CHECK-NEXT: ret i256 -316287267428118237672817312187119211596090618561856315618061562

  %res = call i256 @__sar(i256 1, i256 -632574534856236475345634624374238423192181237123712631236123123)
  ret i256 %res
}

define i256 @test_sar15() {
; CHECK-LABEL: @test_sar15
; CHECK-NEXT: ret i256 28948022309329048855892746252171976963317496166410141009864396001978282409983

  %res = call i256 @__sar(i256 1, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sar16() {
; CHECK-LABEL: @test_sar16
; CHECK-NEXT: ret i256 -28948022309329048855892746252171976963317496166410141009864396001978282409984

  %res = call i256 @__sar(i256 1, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sar17() {
; CHECK-LABEL: @test_sar17
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 40, i256 0)
  ret i256 %res
}

define i256 @test_sar18() {
; CHECK-LABEL: @test_sar18
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 40, i256 1)
  ret i256 %res
}

define i256 @test_sar19() {
; CHECK-LABEL: @test_sar19
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 40, i256 -1)
  ret i256 %res
}

define i256 @test_sar20() {
; CHECK-LABEL: @test_sar20
; CHECK-NEXT: ret i256 34260857756004221344198816653844212619488550546528761082076738620

  %res = call i256 @__sar(i256 40, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_sar21() {
; CHECK-LABEL: @test_sar21
; CHECK-NEXT: ret i256 -575323187928221591706128151030714819346195542606177

  %res = call i256 @__sar(i256 40, i256 -632574534856236475345634624374238423192181237123712631236123123)
  ret i256 %res
}

define i256 @test_sar22() {
; CHECK-LABEL: @test_sar22
; CHECK-NEXT: ret i256 52656145834278593348959013841835216159447547700274555627155488767

  %res = call i256 @__sar(i256 40, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sar23() {
; CHECK-LABEL: @test_sar23
; CHECK-NEXT: ret i256 -52656145834278593348959013841835216159447547700274555627155488768

  %res = call i256 @__sar(i256 40, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sar24() {
; CHECK-LABEL: @test_sar24
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 171, i256 0)
  ret i256 %res
}

define i256 @test_sar25() {
; CHECK-LABEL: @test_sar25
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 171, i256 1)
  ret i256 %res
}

define i256 @test_sar26() {
; CHECK-LABEL: @test_sar26
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 171, i256 -1)
  ret i256 %res
}

define i256 @test_sar27() {
; CHECK-LABEL: @test_sar27
; CHECK-NEXT: ret i256 12585451483284036284784745

  %res = call i256 @__sar(i256 171, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_sar28() {
; CHECK-LABEL: @test_sar28
; CHECK-NEXT: ret i256 -211340361659

  %res = call i256 @__sar(i256 171, i256 -632574534856236475345634624374238423192181237123712631236123123)
  ret i256 %res
}

define i256 @test_sar29() {
; CHECK-LABEL: @test_sar29
; CHECK-NEXT: ret i256 19342813113834066795298815

  %res = call i256 @__sar(i256 171, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sar30() {
; CHECK-LABEL: @test_sar30
; CHECK-NEXT: ret i256 -19342813113834066795298816

  %res = call i256 @__sar(i256 171, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sar31() {
; CHECK-LABEL: @test_sar31
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 254, i256 0)
  ret i256 %res
}

define i256 @test_sar32() {
; CHECK-LABEL: @test_sar32
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 254, i256 1)
  ret i256 %res
}

define i256 @test_sar33() {
; CHECK-LABEL: @test_sar33
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 254, i256 -1)
  ret i256 %res
}

define i256 @test_sar34() {
; CHECK-LABEL: @test_sar34
; CHECK-NEXT: ret i256 1

  %res = call i256 @__sar(i256 254, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_sar35() {
; CHECK-LABEL: @test_sar35
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 254, i256 -632574534856236475345634624374238423192181237123712631236123123)
  ret i256 %res
}

define i256 @test_sar36() {
; CHECK-LABEL: @test_sar36
; CHECK-NEXT: ret i256 1

  %res = call i256 @__sar(i256 254, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sar37() {
; CHECK-LABEL: @test_sar37
; CHECK-NEXT: ret i256 -2

  %res = call i256 @__sar(i256 254, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sar38() {
; CHECK-LABEL: @test_sar38
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 255, i256 0)
  ret i256 %res
}

define i256 @test_sar39() {
; CHECK-LABEL: @test_sar39
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 255, i256 1)
  ret i256 %res
}

define i256 @test_sar40() {
; CHECK-LABEL: @test_sar40
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 255, i256 -1)
  ret i256 %res
}

define i256 @test_sar41() {
; CHECK-LABEL: @test_sar41
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 255, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_sar42() {
; CHECK-LABEL: @test_sar42
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 255, i256 -632574534856236475345634624374238423192181237123712631236123123)
  ret i256 %res
}

define i256 @test_sar43() {
; CHECK-LABEL: @test_sar43
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 255, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sar44() {
; CHECK-LABEL: @test_sar44
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 255, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sar45() {
; CHECK-LABEL: @test_sar45
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 256, i256 0)
  ret i256 %res
}

define i256 @test_sar46() {
; CHECK-LABEL: @test_sar46
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 256, i256 1)
  ret i256 %res
}

define i256 @test_sar47() {
; CHECK-LABEL: @test_sar47
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 256, i256 -1)
  ret i256 %res
}

define i256 @test_sar48() {
; CHECK-LABEL: @test_sar48
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 256, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_sar49() {
; CHECK-LABEL: @test_sar49
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 256, i256 -632574534856236475345634624374238423192181237123712631236123123)
  ret i256 %res
}

define i256 @test_sar50() {
; CHECK-LABEL: @test_sar50
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 256, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sar51() {
; CHECK-LABEL: @test_sar51
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 256, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sar52() {
; CHECK-LABEL: @test_sar52
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 35242523534534534233424343343443, i256 0)
  ret i256 %res
}

define i256 @test_sar53() {
; CHECK-LABEL: @test_sar53
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 35242523534534534233424343343443, i256 1)
  ret i256 %res
}

define i256 @test_sar54() {
; CHECK-LABEL: @test_sar54
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 35242523534534534233424343343443, i256 -1)
  ret i256 %res
}

define i256 @test_sar55() {
; CHECK-LABEL: @test_sar55
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 35242523534534534233424343343443, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_sar56() {
; CHECK-LABEL: @test_sar56
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 35242523534534534233424343343443, i256 -632574534856236475345634624374238423192181237123712631236123123)
  ret i256 %res
}

define i256 @test_sar57() {
; CHECK-LABEL: @test_sar57
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 35242523534534534233424343343443, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sar58() {
; CHECK-LABEL: @test_sar58
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 35242523534534534233424343343443, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

define i256 @test_sar59() {
; CHECK-LABEL: @test_sar59
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 -1, i256 0)
  ret i256 %res
}

define i256 @test_sar60() {
; CHECK-LABEL: @test_sar60
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 -1, i256 1)
  ret i256 %res
}

define i256 @test_sar61() {
; CHECK-LABEL: @test_sar61
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 -1, i256 -1)
  ret i256 %res
}

define i256 @test_sar62() {
; CHECK-LABEL: @test_sar62
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 -1, i256 37670211480306196047687443673641227745170897112008692523754794019498533073987)
  ret i256 %res
}

define i256 @test_sar63() {
; CHECK-LABEL: @test_sar63
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 -1, i256 -632574534856236475345634624374238423192181237123712631236123123)
  ret i256 %res
}

define i256 @test_sar64() {
; CHECK-LABEL: @test_sar64
; CHECK-NEXT: ret i256 0

  %res = call i256 @__sar(i256 -1, i256 57896044618658097711785492504343953926634992332820282019728792003956564819967)
  ret i256 %res
}

define i256 @test_sar65() {
; CHECK-LABEL: @test_sar65
; CHECK-NEXT: ret i256 -1

  %res = call i256 @__sar(i256 -1, i256 -57896044618658097711785492504343953926634992332820282019728792003956564819968)
  ret i256 %res
}

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

attributes #0 = { nounwind readnone willreturn }
