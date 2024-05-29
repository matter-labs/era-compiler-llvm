; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

  .text
; define symbols at zero offset from the start of .text section
handler:

foo:

  far_call                      r3, r4, @handler
  far_call.delegate             r3, r4, @handler
  far_call.mimic                r3, r4, @handler

  far_call.static               r3, r4, @handler
  far_call.delegate.static      r3, r4, @handler
  far_call.mimic.static         r3, r4, @handler

  far_call.shard                r3, r4, @handler
  far_call.delegate.shard       r3, r4, @handler
  far_call.mimic.shard          r3, r4, @handler

  far_call.st.sh               r3, r4, @handler
  far_call.delegate.st.sh      r3, r4, @handler
  far_call.mimic.st.sh         r3, r4, @handler

; CHECK:  .text
; CHECK:handler:
; CHECK:foo:

; CHECK:  far_call           r3, r4, @handler                ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x21]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  far_call.delegate  r3, r4, @handler                ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x25]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  far_call.mimic     r3, r4, @handler                ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x29]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8

; CHECK:  far_call.static            r3, r4, @handler        ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x23]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  far_call.delegate.static   r3, r4, @handler        ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x27]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  far_call.mimic.static      r3, r4, @handler        ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x2b]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8

; CHECK:  far_call.shard             r3, r4, @handler        ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x22]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  far_call.delegate.shard    r3, r4, @handler        ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x26]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  far_call.mimic.shard       r3, r4, @handler        ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x2a]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8

; CHECK:  far_call.st.sh                 r3, r4, @handler    ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x24]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  far_call.delegate.st.sh        r3, r4, @handler    ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x28]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
; CHECK:  far_call.mimic.st.sh           r3, r4, @handler    ; encoding: [0x00,0x00,A,A,0x00,0x43,0x04,0x2c]
; CHECK:  ;   fixup A - offset: 2, value: @handler, kind: fixup_16_scale_8
