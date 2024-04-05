; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

; define symbols at zero offsets inside the corresponding sections
  .rodata
jump_table:
  .cell 0

  .text
foo:
label:

  jump       r1
  jump       @label
  jump       stack[r1 + 42]
  jump       stack-[r1 + 42]
  jump       stack-=[r1 + 42]
  jump       code[r1 + 42]

  jump       r1, r2
  jump       @label, r2
  jump       stack[r1 + 42], r2
  jump       stack-[r1 + 42], r2
  jump       stack-=[r1 + 42], r2
  jump       code[r1 + 42], r2

; do not confuse @jump_target and @indirect_via_const[1]
  jump       @label
  jump       @jump_table[1]
  jump       @label, r2
  jump       @jump_table[1], r2

; CHECK:  .text
; CHECK:foo:
; CHECK:label:

; CHECK:  jump  r1                              ; encoding: [0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x39]
; CHECK:  jump  @label                          ; encoding: [0x00,0x00,A,A,0x00,0x00,0x01,0x3d]
; CHECK:  ;   fixup A - offset: 2, value: @label, kind: fixup_16_scale_8
; CHECK:  jump  stack[42 + r1]                  ; encoding: [0x00,0x00,0x00,0x2a,0x00,0x01,0x01,0x3c]
; CHECK:  jump  stack-[42 + r1]                 ; encoding: [0x00,0x00,0x00,0x2a,0x00,0x01,0x01,0x3b]
; CHECK:  jump  stack-=[42 + r1]                ; encoding: [0x00,0x00,0x00,0x2a,0x00,0x01,0x01,0x3a]
; CHECK:  jump  code[r1+42]                     ; encoding: [0x00,0x00,0x00,0x2a,0x00,0x01,0x01,0x3e]

; CHECK:  jump  r1, r2                          ; encoding: [0x00,0x00,0x00,0x00,0x02,0x01,0x01,0x39]
; CHECK:  jump  @label, r2                      ; encoding: [0x00,0x00,A,A,0x02,0x00,0x01,0x3d]
; CHECK:  ;   fixup A - offset: 2, value: @label, kind: fixup_16_scale_8
; CHECK:  jump  stack[42 + r1], r2              ; encoding: [0x00,0x00,0x00,0x2a,0x02,0x01,0x01,0x3c]
; CHECK:  jump  stack-[42 + r1], r2             ; encoding: [0x00,0x00,0x00,0x2a,0x02,0x01,0x01,0x3b]
; CHECK:  jump  stack-=[42 + r1], r2            ; encoding: [0x00,0x00,0x00,0x2a,0x02,0x01,0x01,0x3a]
; CHECK:  jump  code[r1+42], r2                 ; encoding: [0x00,0x00,0x00,0x2a,0x02,0x01,0x01,0x3e]

; CHECK:  jump  @label                          ; encoding: [0x00,0x00,A,A,0x00,0x00,0x01,0x3d]
; CHECK:  ;   fixup A - offset: 2, value: @label, kind: fixup_16_scale_8
; CHECK:  jump  @jump_table[1]                  ; encoding: [0x00,0x00,A,0x01'A',0x00,0x00,0x01,0x3e]
; CHECK:  ;   fixup A - offset: 2, value: @jump_table, kind: fixup_16_scale_32
; CHECK:  jump  @label, r2                      ; encoding: [0x00,0x00,A,A,0x02,0x00,0x01,0x3d]
; CHECK:  ;   fixup A - offset: 2, value: @label, kind: fixup_16_scale_8
; CHECK:  jump  @jump_table[1], r2              ; encoding: [0x00,0x00,A,0x01'A',0x02,0x00,0x01,0x3e]
; CHECK:  ;   fixup A - offset: 2, value: @jump_table, kind: fixup_16_scale_32
