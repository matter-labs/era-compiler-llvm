; RUN: llvm-mc -triple eravm -filetype=asm %s -o - | FileCheck %s

  .text
  .globl test
test:
  add  r1, r0, r4
  add  r0, r0, r1
  sub.s! 10, r3, r5
  jump.le @JTI0_0[r3]
  jump @.BB0_7
.BB0_1:
  add r4, r2, r1
  ret
.BB0_2:
  sub r4, r2, r1
  ret
.BB0_3:
  mul r4, r2, r1, r2
  ret
.BB0_4:
  sub! r2, r0, r1
  jump.eq @.BB0_5
  div r4, r2, r1, r2
.BB0_7:
  ret
.BB0_5:
  add r0, r0, r1
  ret
  .rodata
  .p2align 5, 0x0
JTI0_0:
  .cell	@.BB0_1
  .cell	@.BB0_2
  .cell	@.BB0_3
  .cell	@.BB0_7
  .cell	@.BB0_7
  .cell	@.BB0_7
  .cell	@.BB0_7
  .cell	@.BB0_7
  .cell	@.BB0_7
  .cell	@.BB0_7
  .cell	@.BB0_4

; CHECK:  	.text
; CHECK:  	.globl	test
; CHECK:  test:
; CHECK:  	add	r1, r0, r4
; CHECK:  	add	r0, r0, r1
; CHECK:  	sub.s!	10, r3, r5
; CHECK:  	jump.le	@JTI0_0[r3]
; CHECK:  	jump	@.BB0_7
; CHECK:  .BB0_1:
; CHECK:  	add	r4, r2, r1
; CHECK:  	ret
; CHECK:  .BB0_2:
; CHECK:  	sub	r4, r2, r1
; CHECK:  	ret
; CHECK:  .BB0_3:
; CHECK:  	mul	r4, r2, r1, r2
; CHECK:  	ret
; CHECK:  .BB0_4:
; CHECK:  	sub!	r2, r0, r1
; CHECK:  	jump.eq	@.BB0_5
; CHECK:  	div	r4, r2, r1, r2
; CHECK:  .BB0_7:
; CHECK:  	ret
; CHECK:  .BB0_5:
; CHECK:  	add	r0, r0, r1
; CHECK:  	ret
; CHECK:  	.rodata
; CHECK:  	.p2align	5, 0x0
; CHECK:  JTI0_0:
; CHECK:  	.cell	@.BB0_1
; CHECK:  	.cell	@.BB0_2
; CHECK:  	.cell	@.BB0_3
; CHECK:  	.cell	@.BB0_7
; CHECK:  	.cell	@.BB0_7
; CHECK:  	.cell	@.BB0_7
; CHECK:  	.cell	@.BB0_7
; CHECK:  	.cell	@.BB0_7
; CHECK:  	.cell	@.BB0_7
; CHECK:  	.cell	@.BB0_7
; CHECK:  	.cell	@.BB0_4
