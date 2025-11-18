; RUN: llc -filetype=obj %s -o - | llvm-objdump --no-leading-addr --disassemble - | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i256 @test() {
; CHECK-LABEL:  <test>:
; CHECK:  69 11 22 33 44 55 66 77 88 99 ff     	PUSH10          0x112233445566778899FF
  ret i256 80911113678783024503295
}
