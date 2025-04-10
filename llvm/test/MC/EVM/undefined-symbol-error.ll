; RUN: not --crash llc -filetype=obj --mtriple=evm %s 2>&1 | FileCheck %s

; CHECK: LLVM ERROR: unable to evaluate offset to undefined symbol 'foo'

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

declare void @foo()

define void @bar() {
  call void @foo()
  ret void
}
