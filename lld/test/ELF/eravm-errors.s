; REQUIRES: eravm
; RUN: llvm-mc -filetype=obj -arch=eravm %s -o %t.o
; RUN: llvm-objdump --no-leading-addr --disassemble --reloc --syms %t.o | FileCheck --check-prefix=INPUT %s
; RUN: not ld.lld -T %S/Inputs/eravm.lds %t.o -o %t 2>&1                | FileCheck --check-prefix=ERRORS %s

; LLD should emit an error when value computed for a relocation is not aligned
; at 32 bytes for R_ERAVM_16_SCALE_32 and at 8 bytes for R_ERAVM_16_SCALE_8.

  .data
  .p2align 5

  .type  dummy_var,@object
dummy_var:
  .byte 0

  .type  unaligned_var,@object
  .globl unaligned_var
unaligned_var:
  .byte 1

  .p2align 3
  .type  misaligned_var,@object
  .globl misaligned_var
misaligned_var:
  .byte 2

; INPUT:      SYMBOL TABLE:
; INPUT-NEXT: 00000000 l    d  .text  00000000 .text
; INPUT-NEXT: 00000000 l     O .data  00000000 dummy_var
; INPUT-NEXT: 00000000 l       .text  00000000 reloc_misaligned_data
; INPUT-NEXT: 00000018 l       .text  00000000 reloc_misaligned_jmptarget
; INPUT-NEXT: 00000029 l       .text  00000000 bad_jmptarget
; INPUT-NEXT: 00000001 g     O .data  00000000 unaligned_var
; INPUT-NEXT: 00000008 g     O .data  00000000 misaligned_var

  .text
  .p2align 3
reloc_misaligned_data:
  add stack[@unaligned_var], r1, r1
  add stack[@misaligned_var], r1, r1
  ret
; INPUT-LABEL: <reloc_misaligned_data>:
; INPUT-NEXT:   00 00 00 00 01 10 00 31        add     stack[r0], r1, r1
; INPUT-NEXT:                  R_ERAVM_16_SCALE_32     unaligned_var
; INPUT-NEXT:   00 00 00 00 01 10 00 31        add     stack[r0], r1, r1
; INPUT-NEXT:                  R_ERAVM_16_SCALE_32     misaligned_var
; INPUT-NEXT:   00 00 00 00 00 01 04 2d        ret

reloc_misaligned_jmptarget:
  jump @bad_jmptarget
  ret

; INPUT-LABEL: <reloc_misaligned_jmptarget>:
; INPUT-NEXT:   00 00 00 00 00 00 01 3d        jump    0
; INPUT-NEXT:                  R_ERAVM_16_SCALE_8      .text+0x29
; INPUT-NEXT:   00 00 00 00 00 01 04 2d        ret

  .p2align 3
  .byte 0
bad_jmptarget:
  .byte 0

; ERRORS: ld.lld: error: {{.*}}/eravm-errors.s.tmp.o:(.text+0x2): improper alignment for relocation R_ERAVM_16_SCALE_32: 0x1001 is not aligned to 32 bytes
; ERRORS: ld.lld: error: {{.*}}/eravm-errors.s.tmp.o:(.text+0xa): improper alignment for relocation R_ERAVM_16_SCALE_32: 0x1008 is not aligned to 32 bytes
; ERRORS: ld.lld: error: {{.*}}/eravm-errors.s.tmp.o:(.text+0x1a): improper alignment for relocation R_ERAVM_16_SCALE_8: 0x29 is not aligned to 8 bytes
