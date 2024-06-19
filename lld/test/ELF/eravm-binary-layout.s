; REQUIRES: eravm
; RUN: llvm-mc -filetype=obj -arch=eravm %s -o %t.o
; RUN: ld.lld -T %S/Inputs/eravm-binary-layout.lds %t.o -o %t
; RUN: llvm-objdump --no-leading-addr  --disassemble-all --disassemble-zeroes --reloc --syms %t   | FileCheck --check-prefix=OUTPUT %s
; RUN: ld.lld -T %S/Inputs/eravm-binary-layout-hash.lds %t.o -o %t
; RUN: llvm-objdump --no-leading-addr  --disassemble-all --disassemble-zeroes --reloc --syms %t   | FileCheck --check-prefix=OUTPUT-HASH %s

        .text
	nop	stack+=[2 + r0]
	add	@glob_initializer[0], r0, stack[@glob]

	.globl	get_glob
get_glob:
	add	3, r0, r1
	add	stack[@glob], r1, r1
	ret

DEFAULT_UNWIND:
	ret.panic.to_label @DEFAULT_UNWIND
DEFAULT_FAR_RETURN:
	ret.ok.to_label	r1, @DEFAULT_FAR_RETURN
DEFAULT_FAR_REVERT:
	ret.revert.to_label r1, @DEFAULT_FAR_REVERT

	.data
	.globl	glob                            ; @glob
	.p2align	5, 0x0
glob:
	.cell	113

	.globl	glob_ptr                    ; @glob_ptr
	.p2align	5, 0x0
glob_ptr:
	.cell	0

	.rodata
glob_initializer:
	.cell	113

; OUTPUT:      SYMBOL TABLE:
; OUTPUT-NEXT: 00000040 l       .code	00000000 glob_initializer
; OUTPUT-NEXT: 00000028 l       .code	00000000 DEFAULT_UNWIND
; OUTPUT-NEXT: 00000030 l       .code	00000000 DEFAULT_FAR_RETURN
; OUTPUT-NEXT: 00000038 l       .code	00000000 DEFAULT_FAR_REVERT
; OUTPUT-NEXT: 00000010 g       .code	00000000 get_glob
; OUTPUT-EMPTY:
; OUTPUT-LABEL: <.code>:
; OUTPUT-NEXT: 00 02 00 00 00 00 00 02       	nop	stack+=[2 + r0]
; OUTPUT-NEXT: 00 00 00 02 00 00 00 47       	add	code[2], r0, stack[r0]
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <get_glob>:
; OUTPUT-NEXT: 00 00 00 03 01 00 00 39       	add	3, r0, r1
; OUTPUT-NEXT: 00 00 00 00 01 10 00 31       	add	stack[r0], r1, r1
; OUTPUT-NEXT: 00 00 00 00 00 01 04 2d       	ret
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <DEFAULT_UNWIND>:
; OUTPUT-NEXT: 00 00 00 05 00 00 04 32       	ret.panic.to_label  5
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <DEFAULT_FAR_RETURN>:
; OUTPUT-NEXT: 00 00 00 06 00 01 04 2e       	ret.ok.to_label	r1, 6
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <DEFAULT_FAR_REVERT>:
; OUTPUT-NEXT: 00 00 00 07 00 01 04 30       	ret.revert.to_label	r1, 7
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <glob_initializer>:
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 71

; OUTPUT-HASH: <glob_initializer>:
; OUTPUT-HASH-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-HASH-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-HASH-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-HASH-NEXT: 00 00 00 00 00 00 00 71
; Next 32 bytes are added to make the total .code size
; the even number of words (32 bytes each).
; OUTPUT-HASH-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-HASH-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-HASH-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-HASH-NEXT: 00 00 00 00 00 00 00 00
; Next 32 bytes are the hash value.
; OUTPUT-HASH-NEXT: 3a c2 25 16 8d f5 42 12
; OUTPUT-HASH-NEXT: a2 5c 1c 01 fd 35 be bf
; OUTPUT-HASH-NEXT: ea 40 8f da c2 e3 1d dd
; OUTPUT-HASH-NEXT: 6f 80 a4 bb f9 a5 f1 cb
