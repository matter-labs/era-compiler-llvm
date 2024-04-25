; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

  .text
; define symbols at zero offset from the start of .text section
foo:
label:

; near
  ret
  revert
  panic
  ret.to_label     r1, @label
  revert.to_label  r1, @label
  panic.to_label   r1, @label

; far
  ret.ok      r3
  ret.revert  r3
  ret.panic   r3

; CHECK:  .text
; CHECK:foo:
; CHECK:label:

; CHECK:  ret                                     ; encoding: [0x00,0x00,0x00,0x00,0x00,0x01,0x04,0x2d]
; CHECK:  revert                                  ; encoding: [0x00,0x00,0x00,0x00,0x00,0x01,0x04,0x2f]
; CHECK:  panic                                   ; encoding: [0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x31]
; CHECK:  ret.ok.to_label    r1, @label           ; encoding: [0x00,0x00,A,A,0x00,0x01,0x04,0x2e]
; CHECK:  ;   fixup A - offset: 2, value: @label, kind: fixup_16_scale_8
; CHECK:  ret.revert.to_label r1, @label          ; encoding: [0x00,0x00,A,A,0x00,0x01,0x04,0x30]
; CHECK:  ;   fixup A - offset: 2, value: @label, kind: fixup_16_scale_8
; CHECK:  ret.panic.to_label  r1, @label          ; encoding: [0x00,0x00,A,A,0x00,0x01,0x04,0x32]
; CHECK:  ;   fixup A - offset: 2, value: @label, kind: fixup_16_scale_8

; CHECK:  ret.ok      r3                          ; encoding: [0x00,0x00,0x00,0x00,0x00,0x03,0x04,0x2d]
; CHECK:  ret.revert  r3                          ; encoding: [0x00,0x00,0x00,0x00,0x00,0x03,0x04,0x2f]
; CHECK:  ret.panic   r3                          ; encoding: [0x00,0x00,0x00,0x00,0x00,0x03,0x04,0x31]
