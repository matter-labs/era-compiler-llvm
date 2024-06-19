; REQUIRES: eravm
; RUN: llvm-mc -filetype=obj -arch=eravm %s -o %t.o
; RUN: ld.lld -T %S/Inputs/eravm-binary-layout.lds %t.o -o %t
; RUN: llvm-objdump --no-leading-addr  --disassemble-all --disassemble-zeroes --reloc --syms %t   | FileCheck --check-prefix=OUTPUT %s

        .text
	nop	stack+=[10 + r0]
	add	@glob_initializer[0], r0, stack[@glob]
	add	@glob.arr.as4_initializer[0], r0, stack[@glob.arr.as4]
	add	@glob.arr.as4_initializer[1], r0, stack[@glob.arr.as4 + 1]
	add	@glob.arr.as4_initializer[2], r0, stack[@glob.arr.as4 + 2]
	add	@glob.arr.as4_initializer[3], r0, stack[@glob.arr.as4 + 3]
	add	@glob_ptr_as3_initializer[0], r0, stack[@glob_ptr_as3]
	add	@glob.arr_initializer[0], r0, stack[@glob.arr]
	add	@glob.arr_initializer[1], r0, stack[@glob.arr + 1]
	add	@glob.arr_initializer[2], r0, stack[@glob.arr + 2]
	add	@glob.arr_initializer[3], r0, stack[@glob.arr + 3]

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

	.globl	glob.arr.as4                    ; @glob.arr.as4
	.p2align	5, 0x0
glob.arr.as4:
	.zero	128

	.globl	glob_ptr_as3                    ; @glob_ptr_as3
	.p2align	5, 0x0
glob_ptr_as3:
	.cell	0

	.globl	glob.arr                        ; @glob.arr
	.p2align	5, 0x0
glob.arr:
	.cell	1
	.cell	29
	.cell	37
	.cell	4

	.rodata
glob_initializer:
	.cell	113
glob.arr.as4_initializer:
	.zero	128
glob_ptr_as3_initializer:
	.cell	0
glob.arr_initializer:
	.cell	1
	.cell	29
	.cell	37
	.cell	4

; OUTPUT:      SYMBOL TABLE:
; OUTPUT-NEXT: 000000a0 l       .code	00000000 glob_initializer
; OUTPUT-NEXT: 000000c0 l       .code	00000000 glob.arr.as4_initializer
; OUTPUT-NEXT: 00000140 l       .code	00000000 glob_ptr_as3_initializer
; OUTPUT-NEXT: 00000160 l       .code	00000000 glob.arr_initializer
; OUTPUT-NEXT: 00000070 l       .code	00000000 DEFAULT_UNWIND
; OUTPUT-NEXT: 00000078 l       .code	00000000 DEFAULT_FAR_RETURN
; OUTPUT-NEXT: 00000080 l       .code	00000000 DEFAULT_FAR_REVERT
; OUTPUT-NEXT: 00000058 g       .code	00000000 get_glob
; OUTPUT-EMPTY:
; OUTPUT-LABEL: <.code>:
; OUTPUT-NEXT: 00 0a 00 00 00 00 00 02       	nop	stack+=[10 + r0]
; OUTPUT-NEXT: 00 00 00 05 00 00 00 47       	add	code[5], r0, stack[r0]
; OUTPUT-NEXT: 00 01 00 06 00 00 00 47       	add	code[6], r0, stack[1 + r0]
; OUTPUT-NEXT: 00 02 00 07 00 00 00 47       	add	code[7], r0, stack[2 + r0]
; OUTPUT-NEXT: 00 03 00 08 00 00 00 47       	add	code[8], r0, stack[3 + r0]
; OUTPUT-NEXT: 00 04 00 09 00 00 00 47       	add	code[9], r0, stack[4 + r0]
; OUTPUT-NEXT: 00 05 00 0a 00 00 00 47       	add	code[10], r0, stack[5 + r0]
; OUTPUT-NEXT: 00 06 00 0b 00 00 00 47       	add	code[11], r0, stack[6 + r0]
; OUTPUT-NEXT: 00 07 00 0c 00 00 00 47       	add	code[12], r0, stack[7 + r0]
; OUTPUT-NEXT: 00 08 00 0d 00 00 00 47       	add	code[13], r0, stack[8 + r0]
; OUTPUT-NEXT: 00 09 00 0e 00 00 00 47       	add	code[14], r0, stack[9 + r0]
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <get_glob>:
; OUTPUT-NEXT: 00 00 00 03 01 00 00 39       	add	3, r0, r1
; OUTPUT-NEXT: 00 00 00 00 01 10 00 31       	add	stack[r0], r1, r1
; OUTPUT-NEXT: 00 00 00 00 00 01 04 2d       	ret
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <DEFAULT_UNWIND>:
; OUTPUT-NEXT: 00 00 00 0e 00 00 04 32       	ret.panic.to_label	14
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <DEFAULT_FAR_RETURN>:
; OUTPUT-NEXT: 00 00 00 0f 00 01 04 2e       	ret.ok.to_label	r1, 15
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <DEFAULT_FAR_REVERT>:
; OUTPUT-NEXT: 00 00 00 10 00 01 04 30       	ret.revert.to_label	r1, 16
; Alignment on 32 bytes before the part with initializers.
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <glob_initializer>:
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 71
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <glob.arr.as4_initializer>:
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <glob_ptr_as3_initializer>:
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-EMPTY:
; OUTPUT-NEXT: <glob.arr_initializer>:
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 01
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 1d
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 25
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 04

; Next the 32 bytes are added to make the total .code size
; the even number of words (32 bytes each).
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00
; OUTPUT-NEXT: 00 00 00 00 00 00 00 00

