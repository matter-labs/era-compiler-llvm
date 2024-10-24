; RUN: not llvm-mc -triple eravm -o - < %s 2>&1 > %t.stdout | FileCheck %s
; RUN: FileCheck --check-prefix=STDOUT %s < %t.stdout

; STDOUT:     .text
; STDOUT-NOT: {{.+}}

; operands (2nd input should be register)
  add       r1, 42, r3
  add       r1, stack[r2 + 1], r3
  add       r1, stack-[r2 + 1], r3
  add       r1, code[r2 + 1], r3

; operands (output should be writable)
  add       r1, r2, 42
  add       r1, r2, code[r3 + 1]

; operands (2nd output dhould be register, if any)
  mul r1, r2, r3, stack[r4 + 1]

; correct number of outputs
  add r1, r2, r3, r4
  mul r1, r2, r3

; commutative instructions cannot have swapped variants because
; there are no corresponding opcodes for machine instructions
  add.s     42, r2, r3
  and.s     42, r2, r3
  or.s      42, r2, r3
  xor.s     42, r2, r3
  mul.s     42, r2, r3, r4

; non-arithmetic suffixes should be rejected
  add.i     r1, r2, r3
  add.st    r1, r2, r3
  add.sh    r1, r2, r3
  add.h     r1, r2, r3
  add.ah    r1, r2, r3

; COM: Autogenerated checks below, see README.md.
; CHECK:       <stdin>:{{[0-9]+}}:3: error: cannot parse instruction
; CHECK-NEXT:    add       r1, 42, r3
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: cannot parse instruction
; CHECK-NEXT:    add       r1, stack[r2 + 1], r3
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: cannot parse instruction
; CHECK-NEXT:    add       r1, stack-[r2 + 1], r3
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: cannot parse instruction
; CHECK-NEXT:    add       r1, code[r2 + 1], r3
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:21: error: cannot parse operand
; CHECK-NEXT:    add       r1, r2, 42
; CHECK-NEXT:                      ^
; CHECK:       <stdin>:{{[0-9]+}}:21: error: cannot parse operand
; CHECK-NEXT:    add       r1, r2, code[r3 + 1]
; CHECK-NEXT:                      ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: cannot parse instruction
; CHECK-NEXT:    mul r1, r2, r3, stack[r4 + 1]
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: cannot parse instruction
; CHECK-NEXT:    add r1, r2, r3, r4
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: cannot parse instruction
; CHECK-NEXT:    mul r1, r2, r3
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:13: error: cannot parse operand
; CHECK-NEXT:    add.s     42, r2, r3
; CHECK-NEXT:              ^
; CHECK:       <stdin>:{{[0-9]+}}:13: error: cannot parse operand
; CHECK-NEXT:    and.s     42, r2, r3
; CHECK-NEXT:              ^
; CHECK:       <stdin>:{{[0-9]+}}:13: error: cannot parse operand
; CHECK-NEXT:    or.s      42, r2, r3
; CHECK-NEXT:              ^
; CHECK:       <stdin>:{{[0-9]+}}:13: error: cannot parse operand
; CHECK-NEXT:    xor.s     42, r2, r3
; CHECK-NEXT:              ^
; CHECK:       <stdin>:{{[0-9]+}}:13: error: cannot parse operand
; CHECK-NEXT:    mul.s     42, r2, r3, r4
; CHECK-NEXT:              ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: unknown mnemonic: add.i
; CHECK-NEXT:    add.i     r1, r2, r3
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: unknown mnemonic: add.st
; CHECK-NEXT:    add.st    r1, r2, r3
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: unknown mnemonic: add.sh
; CHECK-NEXT:    add.sh    r1, r2, r3
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: unknown mnemonic: add.h
; CHECK-NEXT:    add.h     r1, r2, r3
; CHECK-NEXT:    ^
; CHECK:       <stdin>:{{[0-9]+}}:3: error: unknown mnemonic: add.ah
; CHECK-NEXT:    add.ah    r1, r2, r3
; CHECK-NEXT:    ^
