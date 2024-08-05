; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

; To simplify comparison of code produced by the LLVM backend with that
; produced by the standalone assembler, all named constant operands are tested
; in this file for all instruction types.
  .rodata
  constant:
  jump_table:

  .text
foo:
  ; commutative
  add    @constant[r1 + 42], r2, r3
  add    @constant[r1 + 42], r2, stack[r3 + 123]
  add    @constant[r1 + 42], r2, stack-[r3 + 123]
  add    @constant[r1 + 42], r2, stack+=[r3 + 123]

  ; commutative (2 outputs)
  mul    @constant[r1 + 42], r2, r3, r4
  mul    @constant[r1 + 42], r2, stack[r3 + 123], r4
  mul    @constant[r1 + 42], r2, stack-[r3 + 123], r4
  mul    @constant[r1 + 42], r2, stack+=[r3 + 123], r4

  ; non-commutative
  sub    @constant[r1 + 42], r2, r3
  sub    @constant[r1 + 42], r2, stack[r3 + 123]
  sub    @constant[r1 + 42], r2, stack-[r3 + 123]
  sub    @constant[r1 + 42], r2, stack+=[r3 + 123]

  ; non-commutative (2 outputs)
  div    @constant[r1 + 42], r2, r3, r4
  div    @constant[r1 + 42], r2, stack[r3 + 123], r4
  div    @constant[r1 + 42], r2, stack-[r3 + 123], r4
  div    @constant[r1 + 42], r2, stack+=[r3 + 123], r4

  ; pointer arithmetics (only swapped version is supported)
  addp.s    @constant[r1 + 42], r2, r3
  addp.s    @constant[r1 + 42], r2, stack[r3 + 123]
  addp.s    @constant[r1 + 42], r2, stack-[r3 + 123]
  addp.s    @constant[r1 + 42], r2, stack+=[r3 + 123]

  ; jumps
  jump       @jump_table[42]
  jump       @jump_table[r1+42]


; CHECK:  .text
; CHECK:  .rodata
; CHECK:constant:
; CHECK:jump_table:
; CHECK:  .text
; CHECK:foo:
; CHECK:  add   @constant[r1+42], r2, r3        ; encoding: [0x00,0x00,A,0x2a'A',0x03,0x21,0x00,0x41]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  add   @constant[r1+42], r2, stack[123 + r3] ; encoding: [0x00,0x7b,A,0x2a'A',0x03,0x21,0x00,0x47]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  add   @constant[r1+42], r2, stack-[123 + r3] ; encoding: [0x00,0x7b,A,0x2a'A',0x03,0x21,0x00,0x45]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  add   @constant[r1+42], r2, stack+=[123 + r3] ; encoding: [0x00,0x7b,A,0x2a'A',0x03,0x21,0x00,0x43]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32

; CHECK:  mul   @constant[r1+42], r2, r3, r4    ; encoding: [0x00,0x00,A,0x2a'A',0x43,0x21,0x00,0xd1]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  mul   @constant[r1+42], r2, stack[123 + r3], r4 ; encoding: [0x00,0x7b,A,0x2a'A',0x43,0x21,0x00,0xd7]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  mul   @constant[r1+42], r2, stack-[123 + r3], r4 ; encoding: [0x00,0x7b,A,0x2a'A',0x43,0x21,0x00,0xd5]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  mul   @constant[r1+42], r2, stack+=[123 + r3], r4 ; encoding: [0x00,0x7b,A,0x2a'A',0x43,0x21,0x00,0xd3]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32

; CHECK:  sub   @constant[r1+42], r2, r3        ; encoding: [0x00,0x00,A,0x2a'A',0x03,0x21,0x00,0x99]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  sub   @constant[r1+42], r2, stack[123 + r3] ; encoding: [0x00,0x7b,A,0x2a'A',0x03,0x21,0x00,0xa5]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  sub   @constant[r1+42], r2, stack-[123 + r3] ; encoding: [0x00,0x7b,A,0x2a'A',0x03,0x21,0x00,0xa1]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  sub   @constant[r1+42], r2, stack+=[123 + r3] ; encoding: [0x00,0x7b,A,0x2a'A',0x03,0x21,0x00,0x9d]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32

; CHECK:  div   @constant[r1+42], r2, r3, r4    ; encoding: [0x00,0x00,A,0x2a'A',0x43,0x21,0x01,0x29]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  div   @constant[r1+42], r2, stack[123 + r3], r4 ; encoding: [0x00,0x7b,A,0x2a'A',0x43,0x21,0x01,0x35]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  div   @constant[r1+42], r2, stack-[123 + r3], r4 ; encoding: [0x00,0x7b,A,0x2a'A',0x43,0x21,0x01,0x31]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  div   @constant[r1+42], r2, stack+=[123 + r3], r4 ; encoding: [0x00,0x7b,A,0x2a'A',0x43,0x21,0x01,0x2d]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32

; CHECK:  addp.s     @constant[r1+42], r2, r3 ; encoding: [0x00,0x00,A,0x2a'A',0x03,0x21,0x03,0x78]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  addp.s     @constant[r1+42], r2, stack[123 + r3] ; encoding: [0x00,0x7b,A,0x2a'A',0x03,0x21,0x03,0x7e]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  addp.s     @constant[r1+42], r2, stack-[123 + r3] ; encoding: [0x00,0x7b,A,0x2a'A',0x03,0x21,0x03,0x7c]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  addp.s     @constant[r1+42], r2, stack+=[123 + r3] ; encoding: [0x00,0x7b,A,0x2a'A',0x03,0x21,0x03,0x7a]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32

; CHECK:  jump  @jump_table[42]                 ; encoding: [0x00,0x00,A,0x2a'A',0x00,0x00,0x01,0x3e]
; CHECK:  ;   fixup A - offset: 2, value: @jump_table, kind: fixup_16_scale_32
; CHECK:  jump  @jump_table[r1+42]              ; encoding: [0x00,0x00,A,0x2a'A',0x00,0x01,0x01,0x3e]
; CHECK:  ;   fixup A - offset: 2, value: @jump_table, kind: fixup_16_scale_32
