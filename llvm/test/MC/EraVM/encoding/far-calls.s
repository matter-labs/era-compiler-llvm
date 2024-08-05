; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

  .text
; define symbols at zero offset from the start of .text section
handler:

foo:

  callf          r3, r4, @handler
  calld          r3, r4, @handler
  callm          r3, r4, @handler

  callf.st       r3, r4, @handler
  calld.st       r3, r4, @handler
  callm.st       r3, r4, @handler

  callf.sh       r3, r4, @handler
  calld.sh       r3, r4, @handler
  callm.sh       r3, r4, @handler

  callf.st.sh    r3, r4, @handler
  calld.st.sh    r3, r4, @handler
  callm.st.sh    r3, r4, @handler

; CHECK:  .text
; CHECK:handler:
; CHECK:foo:

; CHECK:  callf          r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x21]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  calld          r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x25]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  callm          r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x29]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8

; CHECK:  callf.st       r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x23]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  calld.st       r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x27]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  callm.st       r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x2b]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8

; CHECK:  callf.sh       r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x22]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  calld.sh       r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x26]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  callm.sh       r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x2a]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8

; CHECK:  callf.st.sh    r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x24]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  calld.st.sh    r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x28]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  callm.st.sh    r3, r4, @handler  ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x2c]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
