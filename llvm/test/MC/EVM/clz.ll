; RUN: llc -filetype=obj -mattr=+fusaka %s -o - | llvm-objdump --no-leading-addr --disassemble - | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

declare i256 @llvm.ctlz.i256(i256, i1)

define i256 @clz(i256 %arg) {
; CHECK-LABEL:  <clz>:
; CHECK:  1e           	CLZ
  %ctlz = call i256 @llvm.ctlz.i256(i256 %arg, i1 false)
  ret i256 %ctlz
}
