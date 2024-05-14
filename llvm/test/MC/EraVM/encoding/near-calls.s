; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

  .text
; define symbols at zero offset from the start of .text section
callee:
exception_handler:

foo:

; 3-operand form
  call      r3, @callee, @exception_handler
; 2-operand form
  call      r3, @callee
; 1-operand form
  call      @callee

; COM: Added {{[ \t]}} to prevent matching obsolete near_call mnemonic.

; CHECK:  .text
; CHECK:callee:
; CHECK:exception_handler:
; CHECK:foo:

; CHECK:  {{[ \t]}}call     r3, @callee, @exception_handler              ; encoding: [B,B,A,A,0x00,0x03,0x04,0x0f]
; CHECK:  ;   fixup A - offset: 2, value: @callee, kind: fixup_16_scale_8
; CHECK:  ;   fixup B - offset: 0, value: @exception_handler, kind: fixup_16_scale_8
; CHECK:  {{[ \t]}}call     r3,     @callee,        @DEFAULT_UNWIND_DEST ; encoding: [B,B,A,A,0x00,0x03,0x04,0x0f]
; CHECK:  ;   fixup A - offset: 2, value: @callee, kind: fixup_16_scale_8
; CHECK:  ;   fixup B - offset: 0, value: @DEFAULT_UNWIND_DEST, kind: fixup_16_scale_8
; CHECK:  {{[ \t]}}call     @callee,        @DEFAULT_UNWIND_DEST         ; encoding: [B,B,A,A,0x00,0x00,0x04,0x0f]
; CHECK:  ;   fixup A - offset: 2, value: @callee, kind: fixup_16_scale_8
; CHECK:  ;   fixup B - offset: 0, value: @DEFAULT_UNWIND_DEST, kind: fixup_16_scale_8
