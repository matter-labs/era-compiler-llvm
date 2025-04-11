; RUN: llc -O2 -filetype=obj --mtriple=evm %s -o - | llvm-objdump --no-leading-addr --disassemble - | FileCheck %s

; CHECK-LABEL: <test>:
; CHECK:  fe INVALID

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

declare void @llvm.evm.return(ptr addrspace(1), i256)

define void @test() noreturn {
entry:
  tail call void @llvm.evm.return(ptr addrspace(1) noalias nocapture nofree noundef nonnull align 32 null, i256 0)
  unreachable
}
