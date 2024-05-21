; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

; define symbols at zero offsets inside the corresponding sections
  .rodata
constant:
  .cell 42

  .data
variable:
  .cell 123

  .text
foo:

; encoding register numbers
  add     r0, r0, r0
  add     r0, r0, r1
  add     r0, r0, r2
  add     r0, r0, r3
  add     r0, r0, r4
  add     r0, r0, r5
  add     r0, r0, r6
  add     r0, r0, r7
  add     r0, r0, r8
  add     r0, r0, r9
  add     r0, r0, r10
  add     r0, r0, r11
  add     r0, r0, r12
  add     r0, r0, r13
  add     r0, r0, r14
  add     r0, r0, r15

; encoding condition codes
  add     r0, r0, r0
  add.gt  r0, r0, r0
  add.lt  r0, r0, r0
  add.eq  r0, r0, r0
  add.ge  r0, r0, r0
  add.le  r0, r0, r0
  add.ne  r0, r0, r0
  add.gtlt  r0, r0, r0

; src: unsigned 16-bit immediate
  add       17185, r2, r3
; src: code
  add       code[r1], r2, r3
  add       code[17185], r2, r3
  add       code[r1 + 17185], r2, r3
; src: stack
  add       stack[r1], r2, r3
  add       stack[17185], r2, r3
  add       stack[r1 + 17185], r2, r3
; dst: stack
  add       r1, r2, stack[r3]
  add       r1, r2, stack[17185]
  add       r1, r2, stack[r3 + 17185]

; src: code symbol
  add  code[@constant + r1],          r2, r3
  add  code[@constant + 17185],       r2, r3
  add  code[@constant + 17185 + r1],  r2, r3

; src: stack symbol
  add  stack[@variable + r1],         r2, r3
  add  stack[@variable + 17185],      r2, r3
  add  stack[@variable + 17185 + r1], r2, r3

; dst: stack symbol
  add  r1, r2, stack[@variable + r3]
  add  r1, r2, stack[@variable + 17185]
  add  r1, r2, stack[@variable + 17185 + r3]

; CHECK:  .text
; CHECK:foo:

; CHECK:  add  r0, r0, r0                         ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r1                         ; encoding: [0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r2                         ; encoding: [0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r3                         ; encoding: [0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r4                         ; encoding: [0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r5                         ; encoding: [0x00,0x00,0x00,0x00,0x05,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r6                         ; encoding: [0x00,0x00,0x00,0x00,0x06,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r7                         ; encoding: [0x00,0x00,0x00,0x00,0x07,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r8                         ; encoding: [0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r9                         ; encoding: [0x00,0x00,0x00,0x00,0x09,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r10                        ; encoding: [0x00,0x00,0x00,0x00,0x0a,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r11                        ; encoding: [0x00,0x00,0x00,0x00,0x0b,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r12                        ; encoding: [0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r13                        ; encoding: [0x00,0x00,0x00,0x00,0x0d,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r14                        ; encoding: [0x00,0x00,0x00,0x00,0x0e,0x00,0x00,0x19]
; CHECK:  add  r0, r0, r15                        ; encoding: [0x00,0x00,0x00,0x00,0x0f,0x00,0x00,0x19]

; CHECK:  add  r0, r0, r0                         ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x19]
; CHECK:  add.gt       r0, r0, r0                 ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x19]
; CHECK:  add.lt       r0, r0, r0                 ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x19]
; CHECK:  add.eq       r0, r0, r0                 ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0x60,0x19]
; CHECK:  add.ge       r0, r0, r0                 ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x19]
; CHECK:  add.le       r0, r0, r0                 ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0xa0,0x19]
; CHECK:  add.ne       r0, r0, r0                 ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0x19]
; CHECK:  add.gtlt     r0, r0, r0                 ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x19]

; CHECK:  add   17185, r2, r3                     ; encoding: [0x00,0x00,0x43,0x21,0x03,0x20,0x00,0x39]
; CHECK:  add   code[r1], r2, r3                  ; encoding: [0x00,0x00,0x00,0x00,0x03,0x21,0x00,0x41]
; CHECK:  add   code[r0+17185], r2, r3            ; encoding: [0x00,0x00,0x43,0x21,0x03,0x20,0x00,0x41]
; CHECK:  add   code[r1+17185], r2, r3            ; encoding: [0x00,0x00,0x43,0x21,0x03,0x21,0x00,0x41]
; CHECK:  add   stack[r1], r2, r3                 ; encoding: [0x00,0x00,0x00,0x00,0x03,0x21,0x00,0x31]
; CHECK:  add   stack[17185], r2, r3              ; encoding: [0x00,0x00,0x43,0x21,0x03,0x20,0x00,0x31]
; CHECK:  add   stack[17185 + r1], r2, r3         ; encoding: [0x00,0x00,0x43,0x21,0x03,0x21,0x00,0x31]
; CHECK:  add   r1, r2, stack[r3]                 ; encoding: [0x00,0x00,0x00,0x00,0x03,0x21,0x00,0x1f]
; CHECK:  add   r1, r2, stack[17185]              ; encoding: [0x43,0x21,0x00,0x00,0x00,0x21,0x00,0x1f]
; CHECK:  add   r1, r2, stack[17185 + r3]         ; encoding: [0x43,0x21,0x00,0x00,0x03,0x21,0x00,0x1f]

; CHECK:  add   @constant[r1+0], r2, r3             ; encoding: [0x00,0x00,A,A,0x03,0x21,0x00,0x41]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  add   @constant[r0+17185], r2, r3         ; encoding: [0x00,0x00,0x43'A',0x21'A',0x03,0x20,0x00,0x41]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32
; CHECK:  add   @constant[r1+17185], r2, r3         ; encoding: [0x00,0x00,0x43'A',0x21'A',0x03,0x21,0x00,0x41]
; CHECK:  ;   fixup A - offset: 2, value: @constant, kind: fixup_16_scale_32

; CHECK:  add   stack[@variable+0 + r1], r2, r3     ; encoding: [0x00,0x00,A,A,0x03,0x21,0x00,0x31]
; CHECK:  ;   fixup A - offset: 2, value: @variable, kind: fixup_16_scale_32
; CHECK:  add   stack[@variable+17185], r2, r3      ; encoding: [0x00,0x00,0x43'A',0x21'A',0x03,0x20,0x00,0x31]
; CHECK:  ;   fixup A - offset: 2, value: @variable, kind: fixup_16_scale_32
; CHECK:  add   stack[@variable+17185 + r1], r2, r3 ; encoding: [0x00,0x00,0x43'A',0x21'A',0x03,0x21,0x00,0x31]
; CHECK:  ;   fixup A - offset: 2, value: @variable, kind: fixup_16_scale_32

; CHECK:  add   r1, r2, stack[@variable+0 + r3]     ; encoding: [A,A,0x00,0x00,0x03,0x21,0x00,0x1f]
; CHECK:  ;   fixup A - offset: 0, value: @variable, kind: fixup_16_scale_32
; CHECK:  add   r1, r2, stack[@variable+17185]      ; encoding: [0x43'A',0x21'A',0x00,0x00,0x00,0x21,0x00,0x1f]
; CHECK:  ;   fixup A - offset: 0, value: @variable, kind: fixup_16_scale_32
; CHECK:  add   r1, r2, stack[@variable+17185 + r3] ; encoding: [0x43'A',0x21'A',0x00,0x00,0x03,0x21,0x00,0x1f]
; CHECK:  ;   fixup A - offset: 0, value: @variable, kind: fixup_16_scale_32
