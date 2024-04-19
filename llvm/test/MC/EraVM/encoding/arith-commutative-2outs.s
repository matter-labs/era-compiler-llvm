; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

  .text
foo:

; mnemonic x {flag-preserving, flag-setting} -> opcodes
  mul    r1, r2, r3, r4
  mul!   r1, r2, r3, r4

; combinations of operand types -> opcodes, field layout
  mul    r1,               r2, r3, r4
  mul    42,               r2, r3, r4
  mul    code[r1 + 42],    r2, r3, r4
  mul    stack[r1 + 42],   r2, r3, r4
  mul    stack-[r1 + 42],  r2, r3, r4
  mul    stack-=[r1 + 42], r2, r3, r4
  mul    r1,               r2, stack[r3 + 123],   r4
  mul    42,               r2, stack[r3 + 123],   r4
  mul    code[r1 + 42],    r2, stack[r3 + 123],   r4
  mul    stack[r1 + 42],   r2, stack[r3 + 123],   r4
  mul    stack-[r1 + 42],  r2, stack[r3 + 123],   r4
  mul    stack-=[r1 + 42], r2, stack[r3 + 123],   r4
  mul    r1,               r2, stack-[r3 + 123],  r4
  mul    42,               r2, stack-[r3 + 123],  r4
  mul    code[r1 + 42],    r2, stack-[r3 + 123],  r4
  mul    stack[r1 + 42],   r2, stack-[r3 + 123],  r4
  mul    stack-[r1 + 42],  r2, stack-[r3 + 123],  r4
  mul    stack-=[r1 + 42], r2, stack-[r3 + 123],  r4
  mul    r1,               r2, stack+=[r3 + 123], r4
  mul    42,               r2, stack+=[r3 + 123], r4
  mul    code[r1 + 42],    r2, stack+=[r3 + 123], r4
  mul    stack[r1 + 42],   r2, stack+=[r3 + 123], r4
  mul    stack-[r1 + 42],  r2, stack+=[r3 + 123], r4
  mul    stack-=[r1 + 42], r2, stack+=[r3 + 123], r4


; CHECK:  .text
; CHECK:foo:

; CHECK:  mul   r1, r2, r3, r4                  ; encoding: [0x00,0x00,0x00,0x00,0x43,0x21,0x00,0xa9]
; CHECK:  mul!  r1, r2, r3, r4                  ; encoding: [0x00,0x00,0x00,0x00,0x43,0x21,0x00,0xaa]

; CHECK:  mul   r1, r2, r3, r4                              ; encoding: [0x00,0x00,0x00,0x00,0x43,0x21,0x00,0xa9]
; CHECK:  mul   42, r2, r3, r4                              ; encoding: [0x00,0x00,0x00,0x2a,0x43,0x20,0x00,0xc9]
; CHECK:  mul   code[r1+42], r2, r3, r4                     ; encoding: [0x00,0x00,0x00,0x2a,0x43,0x21,0x00,0xd1]
; CHECK:  mul   stack[42 + r1], r2, r3, r4                  ; encoding: [0x00,0x00,0x00,0x2a,0x43,0x21,0x00,0xc1]
; CHECK:  mul   stack-[42 + r1], r2, r3, r4                 ; encoding: [0x00,0x00,0x00,0x2a,0x43,0x21,0x00,0xb9]
; CHECK:  mul   stack-=[42 + r1], r2, r3, r4                ; encoding: [0x00,0x00,0x00,0x2a,0x43,0x21,0x00,0xb1]
; CHECK:  mul   r1, r2, stack[123 + r3], r4                 ; encoding: [0x00,0x7b,0x00,0x00,0x43,0x21,0x00,0xaf]
; CHECK:  mul   42, r2, stack[123 + r3], r4                 ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x20,0x00,0xcf]
; CHECK:  mul   code[r1+42], r2, stack[123 + r3], r4        ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xd7]
; CHECK:  mul   stack[42 + r1], r2, stack[123 + r3], r4     ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xc7]
; CHECK:  mul   stack-[42 + r1], r2, stack[123 + r3], r4    ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xbf]
; CHECK:  mul   stack-=[42 + r1], r2, stack[123 + r3], r4   ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xb7]
; CHECK:  mul   r1, r2, stack-[123 + r3], r4                ; encoding: [0x00,0x7b,0x00,0x00,0x43,0x21,0x00,0xad]
; CHECK:  mul   42, r2, stack-[123 + r3], r4                ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x20,0x00,0xcd]
; CHECK:  mul   code[r1+42], r2, stack-[123 + r3], r4       ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xd5]
; CHECK:  mul   stack[42 + r1], r2, stack-[123 + r3], r4    ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xc5]
; CHECK:  mul   stack-[42 + r1], r2, stack-[123 + r3], r4   ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xbd]
; CHECK:  mul   stack-=[42 + r1], r2, stack-[123 + r3], r4  ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xb5]
; CHECK:  mul   r1, r2, stack+=[123 + r3], r4               ; encoding: [0x00,0x7b,0x00,0x00,0x43,0x21,0x00,0xab]
; CHECK:  mul   42, r2, stack+=[123 + r3], r4               ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x20,0x00,0xcb]
; CHECK:  mul   code[r1+42], r2, stack+=[123 + r3], r4      ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xd3]
; CHECK:  mul   stack[42 + r1], r2, stack+=[123 + r3], r4   ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xc3]
; CHECK:  mul   stack-[42 + r1], r2, stack+=[123 + r3], r4  ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xbb]
; CHECK:  mul   stack-=[42 + r1], r2, stack+=[123 + r3], r4 ; encoding: [0x00,0x7b,0x00,0x2a,0x43,0x21,0x00,0xb3]
