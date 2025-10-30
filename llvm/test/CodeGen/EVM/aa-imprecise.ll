; RUN: opt -passes=aa-eval -aa-pipeline=evm-aa,basic-aa -print-all-alias-modref-info --debug-only=evm-aa -disable-output < %s 2>&1 | FileCheck %s
; REQUIRES: asserts

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

declare void @llvm.evm.return(ptr addrspace(1), i256)
declare void @llvm.memcpy.p1.p1.i256(ptr addrspace(1), ptr addrspace(1), i256, i1 immarg)

; CHECK-LABEL: Function: test_both_imprecise
; CHECK: MayAlias: ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 256 to ptr addrspace(1))
define void @test_both_imprecise(i256 %size) noreturn {
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 256 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 512 to ptr addrspace(1)), i256 %size, i1 false)
  call void @llvm.evm.return(ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), i256 %size)
  unreachable
}

; CHECK-LABEL: Function: test_mayalias1
; CHECK: MayAlias: ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1))
define void @test_mayalias1(i256 %size) noreturn {
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 512 to ptr addrspace(1)), i256 32, i1 false)
  call void @llvm.evm.return(ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1)), i256 %size)
  unreachable
}

; CHECK-LABEL: Function: test_mayalias2
; CHECK: MayAlias: ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1))
define void @test_mayalias2(i256 %size) noreturn {
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 512 to ptr addrspace(1)), i256 32, i1 false)
  call void @llvm.evm.return(ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), i256 %size)
  unreachable
}

; CHECK-LABEL: Function: test_partialalias
; CHECK: MayAlias: ptr addrspace(1) inttoptr (i256 130 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1))
define void @test_partialalias(i256 %size) noreturn {
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 512 to ptr addrspace(1)), i256 32, i1 false)
  call void @llvm.evm.return(ptr addrspace(1) inttoptr (i256 130 to ptr addrspace(1)), i256 %size)
  unreachable
}

; CHECK-LABEL: Function: test_noalias
; CHECK: NoAlias: ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 96 to ptr addrspace(1))
define void @test_noalias(i256 %size) noreturn {
  tail call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 96 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 512 to ptr addrspace(1)), i256 32, i1 false)
  call void @llvm.evm.return(ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), i256 %size)
  unreachable
}
