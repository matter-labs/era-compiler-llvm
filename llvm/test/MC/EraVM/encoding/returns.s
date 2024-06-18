; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

  .text
; define symbols at zero offset from the start of .text section
foo:
label:

; near
  ret
  rev
  pnc
  retl  @label
  revl  @label
  pncl  @label

; far
  ret   r3
  rev   r3
  pnc

; CHECK:  .text
; CHECK:foo:
; CHECK:label:

; CHECK:  ret                                     ; encoding: [0x00,0x00,0x00,0x00,0x00,0x01,0x04,0x2d]
; CHECK:  rev                                     ; encoding: [0x00,0x00,0x00,0x00,0x00,0x01,0x04,0x2f]
; CHECK:  pnc                                     ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x31]
; CHECK:  retl  @label                            ; encoding: [0x00,0x00,A,A,0x00,0x01,0x04,0x2e]
; CHECK:  ;   fixup A - offset: 2, value: @label, kind: fixup_16_scale_8
; CHECK:  revl  @label                            ; encoding: [0x00,0x00,A,A,0x00,0x01,0x04,0x30]
; CHECK:  ;   fixup A - offset: 2, value: @label, kind: fixup_16_scale_8
; CHECK:  pncl  @label                            ; encoding: [0x00,0x00,A,A,0x00,0x00,0x04,0x32]
; CHECK:  ;   fixup A - offset: 2, value: @label, kind: fixup_16_scale_8

; CHECK:  ret   r3                                ; encoding: [0x00,0x00,0x00,0x00,0x00,0x03,0x04,0x2d]
; CHECK:  rev   r3                                ; encoding: [0x00,0x00,0x00,0x00,0x00,0x03,0x04,0x2f]
; CHECK:  pnc                                     ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x31]
