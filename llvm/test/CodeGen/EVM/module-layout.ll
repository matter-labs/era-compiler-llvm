; RUN: not --crash llc -O3 < %s 2>&1 | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

declare void @llvm.evm.return(ptr addrspace(1), i256)

; CHECK: LLVM ERROR: Entry function '__entry' isn't the first function in the module.

define private fastcc i256 @fun_fib(i256 %0) noinline {
entry:
  %res = add i256 %0, 1
  ret i256 %res
}

define void @__entry() noreturn "evm-entry-function" {
entry:
  %fun_res = tail call fastcc i256 @fun_fib(i256 7)
  tail call void @llvm.evm.return(ptr addrspace(1) null, i256 %fun_res)
  unreachable
}
