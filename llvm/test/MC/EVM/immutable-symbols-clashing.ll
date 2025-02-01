; RUN: not --crash llc -O2 -filetype=obj --mtriple=evm %s 2>&1 | FileCheck %s

; CHECK: LLVM ERROR: MC: duplicating immutable label __load_immutable__imm_id.1

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

declare i256 @llvm.evm.loadimmutable(metadata)

define i256 @__load_immutable__imm_id.1() {
  %ret = call i256 @llvm.evm.loadimmutable(metadata !1)
  ret i256 %ret
}

!1 = !{!"imm_id"}
