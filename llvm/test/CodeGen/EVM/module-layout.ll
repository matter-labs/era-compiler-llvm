; RUN: llc -O3 < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

declare void @llvm.evm.return(ptr addrspace(1), i256)

; CHECK-LABEL: __entry:
; CHECK-LABEL: fun_fib:

define private fastcc i256 @fun_fib(i256 %0) noinline {
entry:
  %res = add i256 %0, 1
  ret i256 %res
}

define void @__entry() noreturn {
entry:
  %fun_res = tail call fastcc i256 @fun_fib(i256 7)
  tail call void @llvm.evm.return(ptr addrspace(1) null, i256 %fun_res)
  unreachable
}
