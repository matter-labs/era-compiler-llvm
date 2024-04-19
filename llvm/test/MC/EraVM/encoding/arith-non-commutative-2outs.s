; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

  .text
foo:

; mnemonic x modifiers -> opcodes
  div      r1, r2, r3, r4
  div.s    r1, r2, r3, r4
  div!     r1, r2, r3, r4
  div.s!   r1, r2, r3, r4

; combinations of operand types -> opcodes, field layout
  div    r1,               r2, r3, r4
  div    42,               r2, r3, r4
  div    code[r1 + 42],    r2, r3, r4
  div    stack[r1 + 42],   r2, r3, r4
  div    stack-[r1 + 42],  r2, r3, r4
  div    stack-=[r1 + 42], r2, r3, r4
  div    r1,               r2, stack[r3 + 123],   r4
  div    42,               r2, stack[r3 + 123],   r4
  div    code[r1 + 42],    r2, stack[r3 + 123],   r4
  div    stack[r1 + 42],   r2, stack[r3 + 123],   r4
  div    stack-[r1 + 42],  r2, stack[r3 + 123],   r4
  div    stack-=[r1 + 42], r2, stack[r3 + 123],   r4
  div    r1,               r2, stack-[r3 + 123],  r4
  div    42,               r2, stack-[r3 + 123],  r4
  div    code[r1 + 42],    r2, stack-[r3 + 123],  r4
  div    stack[r1 + 42],   r2, stack-[r3 + 123],  r4
  div    stack-[r1 + 42],  r2, stack-[r3 + 123],  r4
  div    stack-=[r1 + 42], r2, stack-[r3 + 123],  r4
  div    r1,               r2, stack+=[r3 + 123], r4
  div    42,               r2, stack+=[r3 + 123], r4
  div    code[r1 + 42],    r2, stack+=[r3 + 123], r4
  div    stack[r1 + 42],   r2, stack+=[r3 + 123], r4
  div    stack-[r1 + 42],  r2, stack+=[r3 + 123], r4
  div    stack-=[r1 + 42], r2, stack+=[r3 + 123], r4


; CHECK:  .text
; CHECK:foo:

; CHECK:  div     r1, r2, r3, r4                  ; encoding: [0x00,0x00,0x00,0x00,0x43,0x21,0x00,0xd9]
; CHECK:  div.s   r1, r2, r3, r4                  ; encoding: [0x00,0x00,0x00,0x00,0x43,0x21,0x00,0xda]
; CHECK:  div!    r1, r2, r3, r4                  ; encoding: [0x00,0x00,0x00,0x00,0x43,0x21,0x00,0xdb]
; CHECK:  div.s!  r1, r2, r3, r4                  ; encoding: [0x00,0x00,0x00,0x00,0x43,0x21,0x00,0xdc]

; CHECK:  div   r1, r2, r3, r4                              ; encoding: [0x00,0x00,0x00,0x00,0x43,0x21,0x00,0xd9]
; CHECK:  div   42, r2, r3, r4                              ; encoding: [0x00,0x00,0x00,0x2a,0x43,0x20,0x01,0x19]
; CHECK:  div   code[r1+42], r2, r3, r4                     ; encoding: [0x00,0x00,0x00,0x2a,0x43,0x21,0x01,0x29]
; CHECK:  div   stack[42 + r1], r2, r3, r4                  ; encoding: [0x00,0x00,0x00,0x2a,0x43,0x21,0x01,0x09]
; CHECK:  div   stack-[42 + r1], r2, r3, r4                 ; encoding: [0x00,0x00,0x00,0x2a,0x43,0x21,0x00,0xf9]
; CHECK:  div   stack-=[42 + r1], r2, r3, r4                ; encoding: [0x00,0x00,0x00,0x2a,0x43,0x21,0x00,0xe9]
; CHECK:  div   r1, r2, stack[123 + r3], r4                 ; encoding: [0x00,0x7b,0x00,0x00,0x43,0x21,0x00,0xe5]
; CHECK:  div   42, r2, stack[123 + r3], r4                 ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x20,0x01,0x25]
; CHECK:  div   code[r1+42], r2, stack[123 + r3], r4        ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x01,0x35]
; CHECK:  div   stack[42 + r1], r2, stack[123 + r3], r4     ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x01,0x15]
; CHECK:  div   stack-[42 + r1], r2, stack[123 + r3], r4    ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x01,0x05]
; CHECK:  div   stack-=[42 + r1], r2, stack[123 + r3], r4   ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xf5]
; CHECK:  div   r1, r2, stack-[123 + r3], r4                ; encoding: [0x00,0x7b,0x00,0x00,0x43,0x21,0x00,0xe1]
; CHECK:  div   42, r2, stack-[123 + r3], r4                ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x20,0x01,0x21]
; CHECK:  div   code[r1+42], r2, stack-[123 + r3], r4       ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x01,0x31]
; CHECK:  div   stack[42 + r1], r2, stack-[123 + r3], r4    ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x01,0x11]
; CHECK:  div   stack-[42 + r1], r2, stack-[123 + r3], r4   ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x01,0x01]
; CHECK:  div   stack-=[42 + r1], r2, stack-[123 + r3], r4  ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xf1]
; CHECK:  div   r1, r2, stack+=[123 + r3], r4               ; encoding: [0x00,0x7b,0x00,0x00,0x43,0x21,0x00,0xdd]
; CHECK:  div   42, r2, stack+=[123 + r3], r4               ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x20,0x01,0x1d]
; CHECK:  div   code[r1+42], r2, stack+=[123 + r3], r4      ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x01,0x2d]
; CHECK:  div   stack[42 + r1], r2, stack+=[123 + r3], r4   ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x01,0x0d]
; CHECK:  div   stack-[42 + r1], r2, stack+=[123 + r3], r4  ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xfd]
; CHECK:  div   stack-=[42 + r1], r2, stack+=[123 + r3], r4 ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xed]
