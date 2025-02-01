; RUN: llc -O2 -filetype=obj --mtriple=evm %s -o - | llvm-objdump --syms - | FileCheck %s

; CHECK: SYMBOL TABLE:
; CHECK: {{d*}} l .text	00000000 __load_immutable__imm_id.1
; CHECK: {{d*}} l .text	00000000 __load_immutable__imm_id.2

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

declare i256 @llvm.evm.loadimmutable(metadata)

define i256 @foo() {
  %ret = call i256 @llvm.evm.loadimmutable(metadata !1)
  ret i256 %ret
}

define i256 @bar() {
  %ret = call i256 @llvm.evm.loadimmutable(metadata !1)
  ret i256 %ret
}

!1 = !{!"imm_id"}
