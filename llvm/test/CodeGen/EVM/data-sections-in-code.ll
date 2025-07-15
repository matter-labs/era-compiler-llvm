; RUN: llc < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

@.data1 = private unnamed_addr addrspace(4) constant [13 x i8] c"hello world\0A\00"

declare void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(4) noalias nocapture readonly, i256, i1 immarg)

define void @test() noreturn {
; CHECK-LABEL: test:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    JUMPDEST
; CHECK-NEXT:    PUSH1 0xC
; CHECK-NEXT:    PUSH4 @.data1
; CHECK-NEXT:    PUSH0
; CHECK-NEXT:    CODECOPY
; CHECK-NEXT:    PUSH1 0x3
; CHECK-NEXT:    PUSH0
; CHECK-NEXT:    REVERT
; CHECK:         INVALID
; CHECK-LABEL:  .data1:
; CHECK-NEXT:	.asciz	"hello world\n"

  call void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) null, ptr addrspace(4) @.data1, i256 12, i1 false)
  call void @llvm.evm.revert(ptr addrspace(1) null, i256 3)
  unreachable
}
