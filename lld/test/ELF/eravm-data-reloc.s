; REQUIRES: eravm
; RUN: llvm-mc -filetype=obj -arch=eravm %s -o %t.o
; RUN: llvm-objdump --no-leading-addr --disassemble --reloc %t.o | FileCheck --check-prefix=INPUT %s
; RUN: ld.lld %t.o -o %t
; RUN: llvm-objdump --no-leading-addr --disassemble --reloc --syms %t   | FileCheck --check-prefix=OUTPUT %s

  .data
  .p2align 5
; Force non-trivial offsets of other functions.
  .type  dummy_var,@object
dummy_var:
  .cell 0

  .type  scalar_var,@object
  .globl scalar_var
  .type  scalar_var_local,@object
  .local scalar_var_local
scalar_var:
scalar_var_local:
  .cell 5

  .type  array_var,@object
  .globl array_var
  .type  array_var_local,@object
  .local array_var_local
array_var:
array_var_local:
  .cell 6
  .cell 7
  .cell 8

  .rodata
  .p2align 5
; Force non-trivial offsets of other functions.
  .type  dummy_const,@object
dummy_const:
  .cell 1

  .type  scalar_const,@object
  .globl scalar_const
  .type  scalar_const_local,@object
  .local scalar_const_local
scalar_const:
scalar_const_local:
  .cell 123

  .type  array_const,@object
  .globl array_const
  .type  array_const_local,@object
  .local array_const_local
array_const:
array_const_local:
  .cell 12
  .cell 34
  .cell 56

  .text
  .p2align 3
reloc_src_g:
  add @scalar_const[0], r1, r1
  add stack[@scalar_var], r1, r1
  add @array_const[1], r1, r1
  add stack[@array_var + 1], r1, r1
  ret
; INPUT-LABEL: <reloc_src_g>:
; INPUT-NEXT:  00 00 00 00 01 10 00 41        add     code[r0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_const
; INPUT-NEXT:  00 00 00 00 01 10 00 31        add     stack[r0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_var
; INPUT-NEXT:  00 00 00 01 01 10 00 41        add     code[r0+1], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_const
; INPUT-NEXT:  00 00 00 01 01 10 00 31        add     stack[1 + r0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_var
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL: <reloc_src_g>:
; OUTPUT-NEXT:  00 00 00 01 01 10 00 41        add     code[r0+1], r1, r1
; OUTPUT-NEXT:  00 00 00 01 01 10 00 31        add     stack[1 + r0], r1, r1
; OUTPUT-NEXT:  00 00 00 03 01 10 00 41        add     code[r0+3], r1, r1
; OUTPUT-NEXT:  00 00 00 03 01 10 00 31        add     stack[3 + r0], r1, r1
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

reloc_src_l:
  add @scalar_const_local[0], r1, r1
  add stack[@scalar_var_local], r1, r1
  add @array_const_local[1], r1, r1
  add stack[@array_var_local + 1], r1, r1
  ret
; INPUT-LABEL: <reloc_src_l>:
; INPUT-NEXT:  00 00 00 00 01 10 00 41        add     code[r0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .rodata+0x20
; INPUT-NEXT:  00 00 00 00 01 10 00 31        add     stack[r0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x20
; INPUT-NEXT:  00 00 00 01 01 10 00 41        add     code[r0+1], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .rodata+0x40
; INPUT-NEXT:  00 00 00 01 01 10 00 31        add     stack[1 + r0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x40
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL: <reloc_src_l>:
; OUTPUT-NEXT:  00 00 00 01 01 10 00 41        add     code[r0+1], r1, r1
; OUTPUT-NEXT:  00 00 00 01 01 10 00 31        add     stack[1 + r0], r1, r1
; OUTPUT-NEXT:  00 00 00 03 01 10 00 41        add     code[r0+3], r1, r1
; OUTPUT-NEXT:  00 00 00 03 01 10 00 31        add     stack[3 + r0], r1, r1
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

reloc_dst_g:
  add r1, r0, stack[@scalar_var]
  add r1, r0, stack[@array_var + 1]
  ret
; INPUT-LABEL: <reloc_dst_g>:
; INPUT-NEXT:  00 00 00 00 00 01 00 1f        add     r1, r0, stack[r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_var
; INPUT-NEXT:  00 01 00 00 00 01 00 1f        add     r1, r0, stack[1 + r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_var
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL: <reloc_dst_g>:
; OUTPUT-NEXT:  00 01 00 00 00 01 00 1f        add     r1, r0, stack[1 + r0]
; OUTPUT-NEXT:  00 03 00 00 00 01 00 1f        add     r1, r0, stack[3 + r0]
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

reloc_dst_l:
  add r1, r0, stack[@scalar_var_local]
  add r1, r0, stack[@array_var_local + 1]
  ret
; INPUT-LABEL: <reloc_dst_l>:
; INPUT-NEXT:  00 00 00 00 00 01 00 1f        add     r1, r0, stack[r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x20
; INPUT-NEXT:  00 01 00 00 00 01 00 1f        add     r1, r0, stack[1 + r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x40
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL: <reloc_dst_l>:
; OUTPUT-NEXT:  00 01 00 00 00 01 00 1f        add     r1, r0, stack[1 + r0]
; OUTPUT-NEXT:  00 03 00 00 00 01 00 1f        add     r1, r0, stack[3 + r0]
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

reloc_both_g:
  add @scalar_const[0], r1, stack[@array_var + 1]
  add stack[@scalar_var], r1, stack[@array_var + 1]
  add @array_const[1], r1, stack[@scalar_var]
  add stack[@array_var + 1], r1, stack[@scalar_var]
  ret
; INPUT-LABEL: <reloc_both_g>:
; INPUT-NEXT:  00 01 00 00 00 10 00 47        add     code[r0], r1, stack[1 + r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_var
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_const
; INPUT-NEXT:  00 01 00 00 00 10 00 37        add     stack[r0], r1, stack[1 + r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_var
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_var
; INPUT-NEXT:  00 00 00 01 00 10 00 47        add     code[r0+1], r1, stack[r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_var
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_const
; INPUT-NEXT:  00 00 00 01 00 10 00 37        add     stack[1 + r0], r1, stack[r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_var
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_var
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL:<reloc_both_g>:
; OUTPUT-NEXT:  00 03 00 01 00 10 00 47        add     code[r0+1], r1, stack[3 + r0]
; OUTPUT-NEXT:  00 03 00 01 00 10 00 37        add     stack[1 + r0], r1, stack[3 + r0]
; OUTPUT-NEXT:  00 01 00 03 00 10 00 47        add     code[r0+3], r1, stack[1 + r0]
; OUTPUT-NEXT:  00 01 00 03 00 10 00 37        add     stack[3 + r0], r1, stack[1 + r0]
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

reloc_both_l:
  add @scalar_const_local[0], r1, stack[@array_var_local + 1]
  add stack[@scalar_var_local], r1, stack[@array_var_local + 1]
  add @array_const_local[1], r1, stack[@scalar_var_local]
  add stack[@array_var_local + 1], r1, stack[@scalar_var_local]
  ret
; INPUT-LABEL: <reloc_both_l>:
; INPUT-NEXT:  00 01 00 00 00 10 00 47        add     code[r0], r1, stack[1 + r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x40
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .rodata+0x20
; INPUT-NEXT:  00 01 00 00 00 10 00 37        add     stack[r0], r1, stack[1 + r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x40
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x20
; INPUT-NEXT:  00 00 00 01 00 10 00 47        add     code[r0+1], r1, stack[r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x20
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .rodata+0x40
; INPUT-NEXT:  00 00 00 01 00 10 00 37        add     stack[1 + r0], r1, stack[r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x20
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x40
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL:<reloc_both_l>:
; OUTPUT-NEXT:  00 03 00 01 00 10 00 47        add     code[r0+1], r1, stack[3 + r0]
; OUTPUT-NEXT:  00 03 00 01 00 10 00 37        add     stack[1 + r0], r1, stack[3 + r0]
; OUTPUT-NEXT:  00 01 00 03 00 10 00 47        add     code[r0+3], r1, stack[1 + r0]
; OUTPUT-NEXT:  00 01 00 03 00 10 00 37        add     stack[3 + r0], r1, stack[1 + r0]
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret
