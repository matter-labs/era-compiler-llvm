; RUN: llvm-mc -triple eravm -filetype=asm %s -o - | FileCheck %s

  .text
foo:

; General form: name[.s][!][.cond], in exactly this order
  sub       42, r2, r3
  sub!      42, r2, r3
  sub.s     42, r2, r3
  sub.s!    42, r2, r3
  sub.lt    42, r2, r3
  sub!.lt   42, r2, r3
  sub.s.lt  42, r2, r3
  sub.s!.lt 42, r2, r3

; For uniformity, ".s" modifier should be accepted for non-commutative
; instructions with two register input operands.
  sub.s     r1, r2, r3
  sub.s!    r1, r2, r3
  div.s     r1, r2, r3, r4
  div.s!    r1, r2, r3, r4

; For commutative arithmetic instructions, ".s" modifier cannot be encoded
; in opcode
  add       42, r2, r3
  add!      42, r2, r3
  add.lt    42, r2, r3
  add!.lt   42, r2, r3

; Eight condition codes are supported
  add.eq      r1, r2, r3
  add.lt      r1, r2, r3
  add.gt      r1, r2, r3
  add.ne      r1, r2, r3
  add.ge      r1, r2, r3
  add.le      r1, r2, r3
  add.gtlt    r1, r2, r3


; COM: Autogenerated checks below, see README.md.
; CHECK:  .text
; CHECK:foo:

; CHECK:  sub  42, r2, r3
; CHECK:  sub! 42, r2, r3
; CHECK:  sub.s        42, r2, r3
; CHECK:  sub.s!       42, r2, r3
; CHECK:  sub.lt       42, r2, r3
; CHECK:  sub!.lt      42, r2, r3
; CHECK:  sub.s.lt     42, r2, r3
; CHECK:  sub.s!.lt    42, r2, r3

; CHECK:  sub.s        r1, r2, r3
; CHECK:  sub.s!       r1, r2, r3
; CHECK:  div.s        r1, r2, r3, r4
; CHECK:  div.s!       r1, r2, r3, r4

; CHECK:  add  42, r2, r3
; CHECK:  add! 42, r2, r3
; CHECK:  add.lt       42, r2, r3
; CHECK:  add!.lt      42, r2, r3

; CHECK:  add.eq       r1, r2, r3
; CHECK:  add.lt       r1, r2, r3
; CHECK:  add.gt       r1, r2, r3
; CHECK:  add.ne       r1, r2, r3
; CHECK:  add.ge       r1, r2, r3
; CHECK:  add.le       r1, r2, r3
; CHECK:  add.gtlt     r1, r2, r3
