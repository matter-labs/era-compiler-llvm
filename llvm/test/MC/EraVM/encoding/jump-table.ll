; RUN: llc < %s | FileCheck %s
; RUN: llc -filetype=obj -o %t.o < %s
; RUN: llvm-readelf --sections --relocs --syms %t.o | FileCheck --check-prefix=ELF %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @foo(i256 %arg) {
entry:
  switch i256 %arg, label %default [
    i256 1, label %l1
    i256 2, label %l2
    i256 3, label %l3
    i256 4, label %l4
  ]
l1:
  ret i256 123
l2:
  ret i256 234
l3:
  ret i256 345
l4:
  ret i256 456
default:
  ret i256 42
}

; CHECK:     foo:
; Make sure raw values from the jump table are used as-is, so R_ERAVM_16_SCALE_8
; is the right relocation to use for jump table entry emission.
; CHECK:       jump.le @JTI0_0[r1]

; CHECK:       .rodata
; CHECK-NEXT:  .p2align 5, 0x0
; CHECK-NEXT:JTI0_0:
; CHECK-NEXT:  .cell @.BB0_1
; CHECK-NEXT:  .cell @.BB0_2
; CHECK-NEXT:  .cell @.BB0_3
; CHECK-NEXT:  .cell @.BB0_4

; Capture the index of .rodata section
; ELF: Section Headers:
; ELF: [ [[RODATA:[0-9]+]]] .rodata

; JTI0_0 is mentioned as a const operand of *some* instruction
; ELF:      Relocation section '.rela.text' at offset {{0x[0-9a-f]+}} contains {{[0-9]+}} entries:
; ELF-NEXT:  Offset          Info    Type                Sym. Value  Symbol's Name + Addend
; ELF:      {{[0-9a-f]+}}  00000401 R_ERAVM_16_SCALE_32    00000000   .rodata + 0

; JTI0_0 is filled by R_ERAVM_16_SCALE_8 relocations
; ELF:      Relocation section '.rela.rodata' at offset {{0x[0-9a-f]+}} contains 4 entries:
; ELF-NEXT:  Offset     Info    Type                Sym. Value  Symbol's Name + Addend
; ELF-NEXT: 0000001e  00000202 R_ERAVM_16_SCALE_8     00000000   .text + 20
; ELF-NEXT: 0000003e  00000202 R_ERAVM_16_SCALE_8     00000000   .text + 40
; ELF-NEXT: 0000005e  00000202 R_ERAVM_16_SCALE_8     00000000   .text + 50
; ELF-NEXT: 0000007e  00000202 R_ERAVM_16_SCALE_8     00000000   .text + 60

; JTI0_0 starts at zero offset inside .rodata, as expected by the above checks
; ELF: Symbol table '.symtab' contains {{[0-9]+}} entries:
; ELF:        Num:    Value  Size Type    Bind   Vis       Ndx      Name
; ELF: {{[0-9]+}}: 00000000     0 NOTYPE  LOCAL  DEFAULT [[RODATA]] JTI0_0
