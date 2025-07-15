; RUN: llc < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

@data1 = private unnamed_addr addrspace(4) constant [13 x i8] c"hello world\0A\00"
@data2 = private unnamed_addr addrspace(4) constant [13 x i8] c"hello world\0A\00"
@data3 = private unnamed_addr addrspace(4) constant [5 x i8] c"world"
@data4 = private unnamed_addr addrspace(4) constant [7 x i8] c"another"

declare void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(4) noalias nocapture readonly, i256, i1 immarg)

define void @test() noreturn {
; CHECK-LABEL: test:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    JUMPDEST
; CHECK-NEXT:    PUSH1 0xC
; CHECK-NEXT:    PUSH4 @code_data_section
; CHECK-NEXT:    PUSH0
; CHECK-NEXT:    CODECOPY
; CHECK-NEXT:    PUSH1 0xD
; CHECK-NEXT:    PUSH4 @code_data_section
; CHECK-NEXT:    PUSH1 0x20
; CHECK-NEXT:    CODECOPY
; CHECK-NEXT:    PUSH1 0x5
; CHECK-NEXT:    PUSH4 @code_data_section+6
; CHECK-NEXT:    PUSH1 0x40
; CHECK-NEXT:    CODECOPY
; CHECK-NEXT:    PUSH1 0x7
; CHECK-NEXT:    PUSH4 @code_data_section+13
; CHECK-NEXT:    PUSH1 0x60
; CHECK-NEXT:    CODECOPY
; CHECK-NEXT:    PUSH1 0x80
; CHECK-NEXT:    PUSH0
; CHECK-NEXT:    REVERT
; CHECK:         INVALID
; CHECK-LABEL:  code_data_section:
; CHECK-NEXT:    .byte 0x68, 0x65, 0x6c, 0x6c
; CHECK-NEXT:    .byte 0x6f, 0x20, 0x77, 0x6f
; CHECK-NEXT:    .byte 0x72, 0x6c, 0x64, 0x0a
; CHECK-NEXT:    .byte 0x00, 0x61, 0x6e, 0x6f
; CHECK-NEXT:    .byte 0x74, 0x68, 0x65, 0x72

  call void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) null, ptr addrspace(4) @data1, i256 12, i1 false)
  %dst2 = inttoptr i256 32 to ptr addrspace(1)
  call void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) %dst2, ptr addrspace(4) @data2, i256 13, i1 false)
  %dst3 = inttoptr i256 64 to ptr addrspace(1)
  call void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) %dst3, ptr addrspace(4) @data3, i256 5, i1 false)
  %dst4 = inttoptr i256 96 to ptr addrspace(1)
  call void @llvm.memcpy.p1.p4.i256(ptr addrspace(1) %dst4, ptr addrspace(4) @data4, i256 7, i1 false)
  call void @llvm.evm.revert(ptr addrspace(1) null, i256 128)
  unreachable
}
