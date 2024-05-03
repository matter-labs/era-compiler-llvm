; RUN: llvm-mc -arch=eravm -filetype=obj -o %t.o < %s
; RUN: llvm-readelf --symbols %t.o | FileCheck %s
; RUN: llvm-readelf --hex-dump=.rodata %t.o | FileCheck --check-prefix=RODATA %s
; RUN: llvm-readelf --hex-dump=.data   %t.o | FileCheck --check-prefix=DATA   %s

  .rodata
a:
  .byte 42
  .cell -1  ; implicitly aligned at 32-byte boundary

  .globl global_const
  .type  global_const,@object
global_const:
  .cell  0xaabbccddeeff00112233445566778899abcdef0123456789abcdef0123456789

  .local local_const
  .type  local_const,@object
local_const:
  .cell 123

  .data
b:
  .cell  0xabcdef0123456789abcdef0123456789aabbccddeeff00112233445566778899

  .globl global_var
  .type  global_var,@object
global_var:
  .cell 42
  .cell -1
  .cell 0

  .local local_var
  .type  local_var,@object
local_var:
  .cell 321

; CHECK:      Symbol table '.symtab' contains 7 entries:
; CHECK-NEXT:    Num:    Value  Size Type    Bind   Vis       Ndx         Name
; CHECK-NEXT:      0: 00000000     0 NOTYPE  LOCAL  DEFAULT   UND
; CHECK-NEXT:      1: 00000000     0 NOTYPE  LOCAL  DEFAULT [[RO:[0-9]+]] a
; CHECK-NEXT:      2: 00000060     0 OBJECT  LOCAL  DEFAULT [[RO]]        local_const
; CHECK-NEXT:      3: 00000000     0 NOTYPE  LOCAL  DEFAULT [[RW:[0-9]+]] b
; CHECK-NEXT:      4: 00000080     0 OBJECT  LOCAL  DEFAULT [[RW]]        local_var
; CHECK-NEXT:      5: 00000040     0 OBJECT  GLOBAL DEFAULT [[RO]]        global_const
; CHECK-NEXT:      6: 00000020     0 OBJECT  GLOBAL DEFAULT [[RW]]        global_var

; RODATA:      Hex dump of section '.rodata':
; RODATA-NEXT: 0x00000000 2a000000 00000000 00000000 00000000 *...............
; RODATA-NEXT: 0x00000010 00000000 00000000 00000000 00000000 ................
; RODATA-NEXT: 0x00000020 ffffffff ffffffff ffffffff ffffffff ................
; RODATA-NEXT: 0x00000030 ffffffff ffffffff ffffffff ffffffff ................
; RODATA-NEXT: 0x00000040 aabbccdd eeff0011 22334455 66778899 ........"3DUfw..
; RODATA-NEXT: 0x00000050 abcdef01 23456789 abcdef01 23456789 ....#Eg.....#Eg.
; RODATA-NEXT: 0x00000060 00000000 00000000 00000000 00000000 ................
; RODATA-NEXT: 0x00000070 00000000 00000000 00000000 0000007b ...............{

; DATA:      Hex dump of section '.data':
; DATA-NEXT: 0x00000000 abcdef01 23456789 abcdef01 23456789 ....#Eg.....#Eg.
; DATA-NEXT: 0x00000010 aabbccdd eeff0011 22334455 66778899 ........"3DUfw..
; DATA-NEXT: 0x00000020 00000000 00000000 00000000 00000000 ................
; DATA-NEXT: 0x00000030 00000000 00000000 00000000 0000002a ...............*
; DATA-NEXT: 0x00000040 ffffffff ffffffff ffffffff ffffffff ................
; DATA-NEXT: 0x00000050 ffffffff ffffffff ffffffff ffffffff ................
; DATA-NEXT: 0x00000060 00000000 00000000 00000000 00000000 ................
; DATA-NEXT: 0x00000070 00000000 00000000 00000000 00000000 ................
; DATA-NEXT: 0x00000080 00000000 00000000 00000000 00000000 ................
; DATA-NEXT: 0x00000090 00000000 00000000 00000000 00000141 ...............A
