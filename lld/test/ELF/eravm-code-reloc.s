; REQUIRES: eravm
; RUN: llvm-mc -filetype=obj -arch=eravm %s -o %t.o
; RUN: llvm-objdump --no-leading-addr --disassemble --reloc %t.o | FileCheck --check-prefix=INPUT %s
; RUN: ld.lld %t.o -o %t
; RUN: llvm-objdump --no-leading-addr --disassemble --reloc --syms %t   | FileCheck --check-prefix=OUTPUT %s

  .rodata
  .p2align 5
; Force non-trivial offset of other symbols.
  .type  dummy_var,@object
  .local dummy_var
dummy_var:
  .cell 0

  .type  jump_table,@object
  .globl jump_table
  .type  jump_table_local,@object
  .local jump_table_local
jump_table:
jump_table_local:
  .cell 1
  .cell 2
  .cell 3

  .text
  .p2align 3
; Force non-trivial offsets of other functions.
  .type dummy_function,@function
dummy_function:
  add r1, r2, r3
  ret

; Define @handler and @foo callees near the beginning of the .text section,
; so that changing caller functions does not affect callee offsets.
  .type handler,@function
  .globl handler
  .type handler_local,@function
  .local handler_local
handler:
handler_local:
  ret

  .type foo,@function
  .globl foo
  .type foo_local,@function
  .local foo_local
foo:
foo_local:
  ret

; Test relocations referring global symbols.
  .type caller_g,@function
  .globl caller_g
caller_g:
  near_call r1, @foo, @handler
  far_call  r3, r4, @foo
  jump @label
  ret.to_label    r1, @label
  revert.to_label r1, @label
  panic.to_label  r1, @label
  .globl label
label:
  jump @jump_table[1]
  ret
; INPUT-LABEL: <caller_g>:
; INPUT-NEXT:  00 00 00 00 00 01 04 0f        near_call       r1, 0, 0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      handler
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      foo
; INPUT-NEXT:  00 00 00 00 00 43 04 21        far_call        r3, r4, 0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      foo
; INPUT-NEXT:  00 00 00 00 00 00 01 3d        jump    0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      label
; INPUT-NEXT:  00 00 00 00 00 01 04 2e        ret.ok.to_label r1, 0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      label
; INPUT-NEXT:  00 00 00 00 00 01 04 30        ret.revert.to_label     r1, 0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      label
; INPUT-NEXT:  00 00 00 00 00 01 04 32        ret.panic.to_label      r1, 0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      label
; INPUT-LABEL: <label>:
; INPUT-NEXT:  00 00 00 01 00 00 01 3e        jump    code[r0+1]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     jump_table
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL: <caller_g>:
; OUTPUT-NEXT:  00 02 00 03 00 01 04 0f        near_call       r1, 3, 2
; OUTPUT-NEXT:  00 00 00 03 00 43 04 21        far_call        r3, r4, 3
; OUTPUT-NEXT:  00 00 00 0a 00 00 01 3d        jump    10
; OUTPUT-NEXT:  00 00 00 0a 00 01 04 2e        ret.ok.to_label r1, 10
; OUTPUT-NEXT:  00 00 00 0a 00 01 04 30        ret.revert.to_label     r1, 10
; OUTPUT-NEXT:  00 00 00 0a 00 01 04 32        ret.panic.to_label      r1, 10
; OUTPUT-LABEL: <label>:
; OUTPUT-NEXT:  00 00 00 02 00 00 01 3e        jump    code[r0+2]
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; Test relocations referring local symbols.
  .type caller_l,@function
  .globl caller_l
caller_l:
  near_call r1, @foo_local, @handler_local
  far_call  r3, r4, @foo_local
  jump @label_local
  ret.to_label    r1, @label_local
  revert.to_label r1, @label_local
  panic.to_label  r1, @label_local
  .local label_local
label_local:
  jump @jump_table_local[1]
  ret

; INPUT-LABEL: <caller_l>:
; INPUT-NEXT:  00 00 00 00 00 01 04 0f        near_call       r1, 0, 0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      .text+0x10
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      .text+0x18
; INPUT-NEXT:  00 00 00 00 00 43 04 21        far_call        r3, r4, 0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      .text+0x18
; INPUT-NEXT:  00 00 00 00 00 00 01 3d        jump    0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      .text+0x90
; INPUT-NEXT:  00 00 00 00 00 01 04 2e        ret.ok.to_label r1, 0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      .text+0x90
; INPUT-NEXT:  00 00 00 00 00 01 04 30        ret.revert.to_label     r1, 0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      .text+0x90
; INPUT-NEXT:  00 00 00 00 00 01 04 32        ret.panic.to_label      r1, 0
; INPUT-NEXT:                 R_ERAVM_16_SCALE_8      .text+0x90
; INPUT-LABEL: <label_local>:
; INPUT-NEXT:  00 00 00 01 00 00 01 3e        jump    code[r0+1]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .rodata+0x20
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL: <caller_l>:
; OUTPUT-NEXT:  00 02 00 03 00 01 04 0f        near_call       r1, 3, 2
; OUTPUT-NEXT:  00 00 00 03 00 43 04 21        far_call        r3, r4, 3
; OUTPUT-NEXT:  00 00 00 12 00 00 01 3d        jump    18
; OUTPUT-NEXT:  00 00 00 12 00 01 04 2e        ret.ok.to_label r1, 18
; OUTPUT-NEXT:  00 00 00 12 00 01 04 30        ret.revert.to_label     r1, 18
; OUTPUT-NEXT:  00 00 00 12 00 01 04 32        ret.panic.to_label      r1, 18
; OUTPUT-LABEL: <label_local>:
; OUTPUT-NEXT:  00 00 00 02 00 00 01 3e        jump    code[r0+2]
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret
