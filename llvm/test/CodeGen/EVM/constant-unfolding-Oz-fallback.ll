; RUN: llc -O3 --debug-only=evm-constant-unfolding -evm-bytecode-sizelimit=80 < %s 2>&1 | FileCheck %s
; REQUIRES: asserts
target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

; CHECK: *** Running constant unfolding in the default mode ***
; CHECK-NEXT: *** Initial module size: 136 ***
; CHECK: Checking PUSH32_S i256 35408467139433450592217433187231851964531694900788300625387963629091585785856
; CHECK: Checking PUSH27_S i256 52656145834278593348959013841835216134069776894924259991723442175
; CHECK: Skipping identity transformation
; CHECK: Checking PUSH27_S i256 52656145834278593348959013841835216132831836855638879716824317951
; CHECK: Skipping identity transformation

; CHECK: *** Current module size is 111, which still exceeds the limit, falling back to size-minimization mode ***
; CHECK-NEXT: *** Running constant unfolding in size-minimization mode at loop depth 0 ***

; CHECK: *** Current module size is 111 ***
; CHECK-NEXT: *** Running constant unfolding in size-minimization mode at loop depth 1 ***
; CHECK:  Checking PUSH27_S i256 52656145834278593348959013841835216134069776894924259991723442175

; CHECK: *** Current module size is 103 ***
; CHECK-NEXT: *** Running constant unfolding in size-minimization mode at loop depth 2 ***
; CHECK: Checking PUSH27_S i256 52656145834278593348959013841835216132831836855638879716824317951

; CHECK: *** Current module size is 95 ***

define i256 @test(i256 %p) {

; CHECK-LABEL: .BB0_2:
; CHECK:       PUSH12          0x520000000000000000000000
; CHECK-NEXT:  NOT
; CHECK-NEXT:  PUSH1           0x29
; CHECK-NEXT:  SHL
; CHECK-NEXT:  PUSH1           0x29
; CHECK-NEXT:  SHR

; CHECK-LABEL: .BB0_4:
; CHECK:       PUSH4           0x4E487B71
; CHECK-NEXT:  PUSH1           0xE0
; CHECK-NEXT:  SHL

; CHECK-LABEL: .BB0_6:
; CHECK:       PUSH12          0x560000000000000000000000
; CHECK-NEXT:  NOT
; CHECK-NEXT:  PUSH1           0x29
; CHECK-NEXT:  SHL
; CHECK-NEXT:  PUSH1           0x29
; CHECK-NEXT:  SHR

entry:
  %p.cmp = icmp eq i256 %p, 35
  br i1 %p.cmp, label %exit, label %outer.cond

outer.cond:
  %i = phi i256 [0, %entry], [%i.next, %outer.inc]
  %i.cmp = icmp slt i256 %i, 52656145834278593348959013841835216134069776894924259991723442176
  br i1 %i.cmp, label %inner.cond, label %exit

inner.cond:
  %j = phi i256 [0, %outer.cond], [%j.next, %inner.inc]
  %j.cmp = icmp slt i256 %j, 52656145834278593348959013841835216132831836855638879716824317952
  br i1 %j.cmp, label %inner.body, label %outer.inc

inner.body:
  br label %inner.inc

inner.inc:
  %j.next = add i256 %j, 1
  br label %inner.cond

outer.inc:
  %i.next = add i256 %i, 1
  br label %outer.cond

exit:
  ret i256 35408467139433450592217433187231851964531694900788300625387963629091585785856

}
