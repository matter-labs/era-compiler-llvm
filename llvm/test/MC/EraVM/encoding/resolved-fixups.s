; RUN: llvm-mc -arch=eravm -filetype=obj -o %t.o < %s
; RUN: llvm-readelf --relocs --hex-dump=.text %t.o | FileCheck %s
; RUN: not llvm-mc -arch=eravm -filetype=obj -o /dev/null --defsym=ERR=1 < %s 2>&1 | FileCheck --check-prefix=ERR %s

; Check that resolved fixups are scaled and added to the big-endian field.

  .set ABS8, 24
  .set ABS32, 96
  .set UNALIGNED, 25
  .set BIG, 2097152

  .text
foo:
  jump       @ABS8
  add        code[@ABS32], r0, r1
  add        code[@ABS32+1], r0, r1

.ifdef ERR
; ERR: error: fixup value is not a multiple of 8
  jump       @UNALIGNED
; ERR: error: fixup value is out of range
  add        code[@BIG], r0, r1
; ERR: error: fixup value is out of range
  add        code[@ABS32+65535], r0, r1
.endif

; CHECK: There are no relocations in this file.
; CHECK:      0x00000000 00000003 0000013d 00000003 01000041
; CHECK-NEXT: 0x00000010 00000004 01000041
