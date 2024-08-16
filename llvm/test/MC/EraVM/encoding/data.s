; RUN: llvm-mc -arch=eravm -filetype=obj -o %t.o < %s
; RUN: llvm-readelf --relocs --symbols %t.o | FileCheck %s
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

jump_table:
  .cell 1
  .cell 2

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

  .text
  .globl foo
  .type  foo,@function
foo:
  add code[@global_const], r2, stack[@global_var + 2]
  add code[@local_const], r2, stack[@local_var + 1]
  jump code[@jump_table + 1]
  ret

.rodata
.linker_symbol:
  .linker_symbol_cell  @__$9d134d75c24f6705416dcd739f310469be$__
.section  ".linker_symbol_name__$9d134d75c24f6705416dcd739f310469be$__","S",@progbits
  .ascii  "/()`~!@#$%^&*-+=|\\{}[ ]:;'<>,?/_library:id2"


; CHECK:      Relocation section '.rela.text' at offset {{0x[0-9a-f]+}} contains 5 entries:
; CHECK-NEXT:  Offset     Info    Type                Sym. Value  Symbol's Name + Addend
; CHECK-NEXT: 00000000  00000901 R_ERAVM_16_SCALE_32    00000020   global_var + 0
; CHECK-NEXT: 00000002  00000801 R_ERAVM_16_SCALE_32    00000040   global_const + 0
; CHECK-NEXT: 00000008  00000501 R_ERAVM_16_SCALE_32    00000000   .data + 80
; CHECK-NEXT: 0000000a  00000101 R_ERAVM_16_SCALE_32    00000000   .rodata + 60
; CHECK-NEXT: 00000012  00000101 R_ERAVM_16_SCALE_32    00000000   .rodata + 80

; CHECK:     Relocation section '.rela.rodata' at offset {{0x[0-9a-f]+}} contains 5 entries:
; CHECK-NEXT: Offset     Info    Type                Sym. Value  Symbol's Name + Addend
; CHECK-NEXT: 000000cc  00000b03 R_ERAVM_32             00000000   __$9d134d75c24f6705416dcd739f310469be$__0 + 0
; CHECK-NEXT: 000000d0  00000c03 R_ERAVM_32             00000000   __$9d134d75c24f6705416dcd739f310469be$__1 + 0
; CHECK-NEXT: 000000d4  00000d03 R_ERAVM_32             00000000   __$9d134d75c24f6705416dcd739f310469be$__2 + 0
; CHECK-NEXT: 000000d8  00000e03 R_ERAVM_32             00000000   __$9d134d75c24f6705416dcd739f310469be$__3 + 0
; CHECK-NEXT: 000000dc  00000f03 R_ERAVM_32             00000000   __$9d134d75c24f6705416dcd739f310469be$__4 + 0

; CHECK:      Symbol table '.symtab' contains 16 entries:
; CHECK-NEXT:    Num:    Value  Size Type    Bind   Vis       Ndx         Name
; CHECK-NEXT:      0: 00000000     0 NOTYPE  LOCAL  DEFAULT   UND
; CHECK-NEXT:      1: 00000000     0 SECTION LOCAL  DEFAULT [[RO:[0-9]+]] .rodata
; CHECK-NEXT:      2: 00000000     0 NOTYPE  LOCAL  DEFAULT [[RO]]        a
; CHECK-NEXT:      3: 00000060     0 OBJECT  LOCAL  DEFAULT [[RO]]        local_const
; CHECK-NEXT:      4: 00000080     0 NOTYPE  LOCAL  DEFAULT [[RO]]        jump_table
; CHECK-NEXT:      5: 00000000     0 SECTION LOCAL  DEFAULT [[RW:[0-9]+]] .data
; CHECK-NEXT:      6: 00000000     0 NOTYPE  LOCAL  DEFAULT [[RW]]        b
; CHECK-NEXT:      7: 00000080     0 OBJECT  LOCAL  DEFAULT [[RW]]        local_var
; CHECK-NEXT:      8: 00000040     0 OBJECT  GLOBAL DEFAULT [[RO]]        global_const
; CHECK-NEXT:      9: 00000020     0 OBJECT  GLOBAL DEFAULT [[RW]]        global_var
; CHECK-NEXT:     10: 00000000     0 FUNC    GLOBAL DEFAULT {{[0-9]+}}    fo
; CHECK-NEXT:     11: 00000000     0 NOTYPE  GLOBAL DEFAULT [LINKER_SYMBOL] UND __$9d134d75c24f6705416dcd739f310469be$__0
; CHECK-NEXT:     12: 00000000     0 NOTYPE  GLOBAL DEFAULT [LINKER_SYMBOL] UND __$9d134d75c24f6705416dcd739f310469be$__1
; CHECK-NEXT:     13: 00000000     0 NOTYPE  GLOBAL DEFAULT [LINKER_SYMBOL] UND __$9d134d75c24f6705416dcd739f310469be$__2
; CHECK-NEXT:     14: 00000000     0 NOTYPE  GLOBAL DEFAULT [LINKER_SYMBOL] UND __$9d134d75c24f6705416dcd739f310469be$__3
; CHECK-NEXT:     15: 00000000     0 NOTYPE  GLOBAL DEFAULT [LINKER_SYMBOL] UND __$9d134d75c24f6705416dcd739f310469be$__4


; RODATA:      Hex dump of section '.rodata':
; RODATA-NEXT: 0x00000000 2a000000 00000000 00000000 00000000 *...............
; RODATA-NEXT: 0x00000010 00000000 00000000 00000000 00000000 ................
; RODATA-NEXT: 0x00000020 ffffffff ffffffff ffffffff ffffffff ................
; RODATA-NEXT: 0x00000030 ffffffff ffffffff ffffffff ffffffff ................
; RODATA-NEXT: 0x00000040 aabbccdd eeff0011 22334455 66778899 ........"3DUfw..
; RODATA-NEXT: 0x00000050 abcdef01 23456789 abcdef01 23456789 ....#Eg.....#Eg.
; RODATA-NEXT: 0x00000060 00000000 00000000 00000000 00000000 ................
; RODATA-NEXT: 0x00000070 00000000 00000000 00000000 0000007b ...............{
; RODATA-NEXT: 0x00000080 00000000 00000000 00000000 00000000 ................
; RODATA-NEXT: 0x00000090 00000000 00000000 00000000 00000001 ................
; RODATA-NEXT: 0x000000a0 00000000 00000000 00000000 00000000 ................
; RODATA-NEXT: 0x000000b0 00000000 00000000 00000000 00000002 ................
; RODATA-NEXT: 0x000000c0 00000000 00000000 00000000 00000000 ................
; RODATA-NEXT: 0x000000d0 00000000 00000000 00000000 00000000 ................

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
