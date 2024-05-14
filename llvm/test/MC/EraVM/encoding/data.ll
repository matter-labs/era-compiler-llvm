; RUN: llc < %s | FileCheck %s
; RUN: llc -filetype=obj -o %t.o < %s
; RUN: llvm-readelf --syms --hex-dump=.data %t.o | FileCheck --check-prefix=DATA %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; Test zero and non-zero initializers
@var1 = global i256 0, align 32
@var2 = global i256 42, align 32
@ptr = global ptr addrspace(3) null, align 32

define void @set(ptr addrspace(3) %p, i256 %v1, i256 %v2) nounwind {
  store ptr addrspace(3) %p, ptr @ptr, align 32
  store i256 %v1, ptr @var1, align 32
  store i256 %v2, ptr @var2, align 32
  ret void
}
; CHECK-LABEL: set:
; CHECK:         add     r2, r0, stack[@var1]
; CHECK-NEXT:    ptr.add r1, r0, stack[@ptr]
; CHECK-NEXT:    add     r3, r0, stack[@var2]
; CHECK-NEXT:    ret


define ptr addrspace(3) @get() nounwind {
  %p = load ptr addrspace(3), ptr @ptr, align 32
  %v1 = load i256, ptr @var1, align 32
  %v2 = load i256, ptr @var2, align 32
  %index = mul i256 %v1, %v2
  %res = getelementptr i8, ptr addrspace(3) %p, i256 %index
  ret ptr addrspace(3) %res
}
; CHECK-LABEL: get:
; CHECK:         add     stack[@var2], r0, r1
; CHECK-NEXT:    mul     stack[@var1], r1, r1, r2
; CHECK-NEXT:    ptr.add stack[@ptr], r1, r1
; CHECK-NEXT:    ret

; CHECK:       .data
; CHECK:       .globl var1
; CHECK-NEXT:  .p2align 5, 0x0
; CHECK-NEXT:var1:
; CHECK-NEXT:  .cell 0
; CHECK:       .globl var2
; CHECK-NEXT:  .p2align 5, 0x0
; CHECK-NEXT:var2:
; CHECK-NEXT:  .cell 42
; CHECK:       .globl ptr
; CHECK-NEXT:  .p2align 5, 0x0
; CHECK-NEXT:ptr:
; CHECK-NEXT:  .cell 0

; Check symbol values only, ignore "Num" column as well as unrelated symbols.
; DATA: Symbol table '.symtab' {{.*}}
; DATA: 00000000     0 NOTYPE  GLOBAL DEFAULT     4 var1
; DATA: 00000040     0 NOTYPE  GLOBAL DEFAULT     4 ptr
; DATA: 00000020     0 NOTYPE  GLOBAL DEFAULT     4 var2

; DATA:      Hex dump of section '.data':
; DATA-NEXT: 0x00000000 00000000 00000000 00000000 00000000 ................
; DATA-NEXT: 0x00000010 00000000 00000000 00000000 00000000 ................
; DATA-NEXT: 0x00000020 00000000 00000000 00000000 00000000 ................
; DATA-NEXT: 0x00000030 00000000 00000000 00000000 0000002a ...............*
; DATA-NEXT: 0x00000040 00000000 00000000 00000000 00000000 ................
; DATA-NEXT: 0x00000050 00000000 00000000 00000000 00000000 ................
