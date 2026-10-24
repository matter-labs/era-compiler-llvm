; REQUIRES: eravm
; RUN: llvm-mc -filetype=obj -arch=eravm %s -o %t.o
; RUN: not ld.lld -T %S/Inputs/eravm.lds %t.o -o %t 2>&1 | FileCheck %s

; LLD should emit an error when the scaled value added to the 16-bit field in
; place overflows it, as the assembler does for the fixups it resolves.

  .data
  .p2align 5
  .globl var
var:
  .byte 0

  .text
  add stack[@var+65535], r1, r1
  add stack[@var+65407], r1, r1
  add stack[@var+65408], r1, r1
  ret

; var is at 0x1000, so the scaled value is 128. The second add ends up at 65535,
; which fits.
; CHECK: error: {{.*}}eravm-reloc-overflow.s.tmp.o:(.text+0x2): relocation R_ERAVM_16_SCALE_32 out of range: 65663
; CHECK-NOT: (.text+0xa)
; CHECK: error: {{.*}}eravm-reloc-overflow.s.tmp.o:(.text+0x12): relocation R_ERAVM_16_SCALE_32 out of range: 65536
; CHECK-NOT: error
