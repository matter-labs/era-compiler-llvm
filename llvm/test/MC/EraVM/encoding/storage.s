; RUN: llvm-mc -arch=eravm --show-encoding < %s | FileCheck %s

  .text
foo:

  sload       r3, r4
  sstore      r3, r4

; CHECK:  .text
; CHECK:foo:

; CHECK:  sload   r3, r4                          ; encoding: [0x00,0x00,0x00,0x00,0x04,0x03,0x04,0x1a]
; CHECK:  sstore  r3, r4                          ; encoding: [0x00,0x00,0x00,0x00,0x00,0x43,0x04,0x1b]
