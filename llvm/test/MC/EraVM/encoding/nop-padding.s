; RUN: llvm-mc -arch=eravm -filetype=obj -o %t.o < %s
; RUN: llvm-readelf --hex-dump=.text %t.o | FileCheck %s

; Check that code is padded with EraVM nops.

  .text
foo:
  add        r0, r0, r0
  .p2align   5
  ret

; CHECK:      0x00000000 00000000 00000019 00000000 00000001
; CHECK-NEXT: 0x00000010 00000000 00000001 00000000 00000001
; CHECK-NEXT: 0x00000020 00000000 0001042d
