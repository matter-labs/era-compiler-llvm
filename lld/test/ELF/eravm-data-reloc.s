; REQUIRES: eravm
; RUN: llvm-mc -filetype=obj -arch=eravm %s -o %t.o
; RUN: llvm-objdump --no-leading-addr --disassemble --reloc %t.o | FileCheck --check-prefix=INPUT %s
; RUN: llvm-objdump --no-leading-addr --reloc %t.o | FileCheck --check-prefix=INPUT-LIBSYM %s
; RUN: ld.lld -T %S/Inputs/eravm.lds %t.o -o %t
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

; OUTPUT:      SYMBOL TABLE:
; OUTPUT-NEXT: 00001000 l     O .stack 00000000 dummy_var
; OUTPUT-NEXT: 00001020 l     O .stack 00000000 scalar_var_local
; OUTPUT-NEXT: 00001040 l     O .stack 00000000 array_var_local
; OUTPUT-NEXT: 00000000 l     O .code  00000000 dummy_const
; OUTPUT-NEXT: 00000020 l     O .code  00000000 scalar_const_local
; OUTPUT-NEXT: 00000040 l     O .code  00000000 array_const_local
; OUTPUT-NEXT: 000000c0 l       .code  00000000 reloc_src_g
; OUTPUT-NEXT: 000000e8 l       .code  00000000 reloc_src_l
; OUTPUT-NEXT: 00000110 l       .code  00000000 reloc_dst_g
; OUTPUT-NEXT: 00000128 l       .code  00000000 reloc_dst_l
; OUTPUT-NEXT: 00000140 l       .code  00000000 reloc_both_g
; OUTPUT-NEXT: 00000168 l       .code  00000000 reloc_both_l
; OUTPUT-NEXT: 00001020 g     O .stack 00000000 scalar_var
; OUTPUT-NEXT: 00001040 g     O .stack 00000000 array_var
; OUTPUT-NEXT: 00000020 g     O .code  00000000 scalar_const
; OUTPUT-NEXT: 00000040 g     O .code  00000000 array_const
; OUTPUT-NEXT: 01010101 g       *ABS*	00000000 __$9d134d75c24f6705416dcd739f310469be$__0
; OUTPUT-NEXT: 02020202 g       *ABS*	00000000 __$9d134d75c24f6705416dcd739f310469be$__1
; OUTPUT-NEXT: 03030303 g       *ABS*	00000000 __$9d134d75c24f6705416dcd739f310469be$__2
; OUTPUT-NEXT: 04040404 g       *ABS*	00000000 __$9d134d75c24f6705416dcd739f310469be$__3
; OUTPUT-NEXT: 05050505 g       *ABS*	00000000 __$9d134d75c24f6705416dcd739f310469be$__4

  .text
  .p2align 3
reloc_src_g:
  add code[@scalar_const], r1, r1
  add stack[@scalar_var], r1, r1
  add code[@array_const + 1], r1, r1
  add stack[@array_var + 1], r1, r1
  ret
; INPUT-LABEL: <reloc_src_g>:
; INPUT-NEXT:  00 00 00 00 01 10 00 41        add     code[0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_const
; INPUT-NEXT:  00 00 00 00 01 10 00 31        add     stack[r0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_var
; INPUT-NEXT:  00 00 00 01 01 10 00 41        add     code[1], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_const
; INPUT-NEXT:  00 00 00 01 01 10 00 31        add     stack[1 + r0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_var
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL: <reloc_src_g>:
; OUTPUT-NEXT:  00 00 00 01 01 10 00 41        add     code[1], r1, r1
; OUTPUT-NEXT:  00 00 00 81 01 10 00 31        add     stack[129 + r0], r1, r1
; OUTPUT-NEXT:  00 00 00 03 01 10 00 41        add     code[3], r1, r1
; OUTPUT-NEXT:  00 00 00 83 01 10 00 31        add     stack[131 + r0], r1, r1
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

reloc_src_l:
  add code[@scalar_const_local], r1, r1
  add stack[@scalar_var_local], r1, r1
  add code[@array_const_local + 1], r1, r1
  add stack[@array_var_local + 1], r1, r1
  ret
; INPUT-LABEL: <reloc_src_l>:
; INPUT-NEXT:  00 00 00 00 01 10 00 41        add     code[0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .rodata+0x20
; INPUT-NEXT:  00 00 00 00 01 10 00 31        add     stack[r0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x20
; INPUT-NEXT:  00 00 00 01 01 10 00 41        add     code[1], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .rodata+0x40
; INPUT-NEXT:  00 00 00 01 01 10 00 31        add     stack[1 + r0], r1, r1
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x40
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL: <reloc_src_l>:
; OUTPUT-NEXT:  00 00 00 01 01 10 00 41        add     code[1], r1, r1
; OUTPUT-NEXT:  00 00 00 81 01 10 00 31        add     stack[129 + r0], r1, r1
; OUTPUT-NEXT:  00 00 00 03 01 10 00 41        add     code[3], r1, r1
; OUTPUT-NEXT:  00 00 00 83 01 10 00 31        add     stack[131 + r0], r1, r1
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
; OUTPUT-NEXT:  00 81 00 00 00 01 00 1f        add     r1, r0, stack[129 + r0]
; OUTPUT-NEXT:  00 83 00 00 00 01 00 1f        add     r1, r0, stack[131 + r0]
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
; OUTPUT-NEXT:  00 81 00 00 00 01 00 1f        add     r1, r0, stack[129 + r0]
; OUTPUT-NEXT:  00 83 00 00 00 01 00 1f        add     r1, r0, stack[131 + r0]
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

reloc_both_g:
  add code[@scalar_const], r1, stack[@array_var + 1]
  add stack[@scalar_var], r1, stack[@array_var + 1]
  add code[@array_const + 1], r1, stack[@scalar_var]
  add stack[@array_var + 1], r1, stack[@scalar_var]
  ret
; INPUT-LABEL: <reloc_both_g>:
; INPUT-NEXT:  00 01 00 00 00 10 00 47        add     code[0], r1, stack[1 + r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_var
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_const
; INPUT-NEXT:  00 01 00 00 00 10 00 37        add     stack[r0], r1, stack[1 + r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_var
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_var
; INPUT-NEXT:  00 00 00 01 00 10 00 47        add     code[1], r1, stack[r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_var
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_const
; INPUT-NEXT:  00 00 00 01 00 10 00 37        add     stack[1 + r0], r1, stack[r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     scalar_var
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     array_var
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL:<reloc_both_g>:
; OUTPUT-NEXT:  00 83 00 01 00 10 00 47        add     code[1], r1, stack[131 + r0]
; OUTPUT-NEXT:  00 83 00 81 00 10 00 37        add     stack[129 + r0], r1, stack[131 + r0]
; OUTPUT-NEXT:  00 81 00 03 00 10 00 47        add     code[3], r1, stack[129 + r0]
; OUTPUT-NEXT:  00 81 00 83 00 10 00 37        add     stack[131 + r0], r1, stack[129 + r0]
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

reloc_both_l:
  add code[@scalar_const_local], r1, stack[@array_var_local + 1]
  add stack[@scalar_var_local], r1, stack[@array_var_local + 1]
  add code[@array_const_local + 1], r1, stack[@scalar_var_local]
  add stack[@array_var_local + 1], r1, stack[@scalar_var_local]
  ret
; INPUT-LABEL: <reloc_both_l>:
; INPUT-NEXT:  00 01 00 00 00 10 00 47        add     code[0], r1, stack[1 + r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x40
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .rodata+0x20
; INPUT-NEXT:  00 01 00 00 00 10 00 37        add     stack[r0], r1, stack[1 + r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x40
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x20
; INPUT-NEXT:  00 00 00 01 00 10 00 47        add     code[1], r1, stack[r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x20
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .rodata+0x40
; INPUT-NEXT:  00 00 00 01 00 10 00 37        add     stack[1 + r0], r1, stack[r0]
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x20
; INPUT-NEXT:                 R_ERAVM_16_SCALE_32     .data+0x40
; INPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

; OUTPUT-LABEL:<reloc_both_l>:
; OUTPUT-NEXT:  00 83 00 01 00 10 00 47        add     code[1], r1, stack[131 + r0]
; OUTPUT-NEXT:  00 83 00 81 00 10 00 37        add     stack[129 + r0], r1, stack[131 + r0]
; OUTPUT-NEXT:  00 81 00 03 00 10 00 47        add     code[3], r1, stack[129 + r0]
; OUTPUT-NEXT:  00 81 00 83 00 10 00 37        add     stack[131 + r0], r1, stack[129 + r0]
; OUTPUT-NEXT:  00 00 00 00 00 01 04 2d        ret

  .rodata
.linker_symbol:
  .linker_symbol_cell @__$9d134d75c24f6705416dcd739f310469be$__
.section  ".linker_symbol_name__$9d134d75c24f6705416dcd739f310469be$__","S",@progbits
  .ascii  "/()`~!@#$%^&*-+=|\\{}[ ]:;'<>,?/_library:id2"

; INPUT-LIBSYM: RELOCATION RECORDS FOR [.rodata]:
; INPUT-LIBSYM-NEXT: OFFSET   TYPE                     VALUE
; INPUT-LIBSYM-NEXT: 000000ac R_ERAVM_32               __$9d134d75c24f6705416dcd739f310469be$__0
; INPUT-LIBSYM-NEXT: 000000b0 R_ERAVM_32               __$9d134d75c24f6705416dcd739f310469be$__1
; INPUT-LIBSYM-NEXT: 000000b4 R_ERAVM_32               __$9d134d75c24f6705416dcd739f310469be$__2
; INPUT-LIBSYM-NEXT: 000000b8 R_ERAVM_32               __$9d134d75c24f6705416dcd739f310469be$__3
; INPUT-LIBSYM-NEXT: 000000bc R_ERAVM_32               __$9d134d75c24f6705416dcd739f310469be$__4
