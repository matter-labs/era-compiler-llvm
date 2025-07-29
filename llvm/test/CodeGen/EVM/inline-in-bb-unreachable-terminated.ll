; RUN: opt -passes=inline -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

; CHECK-NOT: define private void @callee_inline
define private void @callee_inline(i256 %arg) {
entry:
  %itptr = inttoptr i256 %arg to ptr addrspace(5)
  %load = load i256, ptr addrspace(5) %itptr, align 1
  %and = and i256 %load, 1461501637330902918203684832716283019655932542975
  %cmp = icmp eq i256 %and, 0
  br i1 %cmp, label %bb1, label %bb2

bb1:
  store i256 32754936060235842233766999496646880210696060726502698976450834618736336437248, ptr addrspace(1) null, align 32
  tail call void @llvm.evm.revert(ptr addrspace(1) null, i256 32)
  unreachable

bb2:
  ret void
}

; CHECK-LABEL: define void @caller_inline1
define void @caller_inline1(i256 %arg) {
entry:
; CHECK-NOT: call void @callee_inline(i256 %arg)
  call void @callee_inline(i256 %arg)
  %itptr = inttoptr i256 %arg to ptr addrspace(5)
  %load = load i256, ptr addrspace(5) %itptr, align 1
  store i256 %load, ptr addrspace(1) null, align 32
  call void @llvm.evm.return(ptr addrspace(1) null, i256 32)
  unreachable
}

; CHECK-LABEL: define void @caller_inline2
define void @caller_inline2(i256 %arg1, i256 %arg2) {
entry:
; CHECK-NOT: call void @callee_inline(i256 %arg2)
  call void @callee_inline(i256 %arg2)
  %itptr = inttoptr i256 %arg2 to ptr addrspace(5)
  %load = load i256, ptr addrspace(5) %itptr, align 1
  store i256 %load, ptr addrspace(1) null, align 32
  call void @llvm.evm.return(ptr addrspace(1) null, i256 32)
  unreachable
}

; CHECK-LABEL: define private void @callee_noinline
define private void @callee_noinline() {
entry:
  store i256 128, ptr addrspace(5) inttoptr (i256 32 to ptr addrspace(5)), align 32
  store i256 128, ptr addrspace(5) inttoptr (i256 64 to ptr addrspace(5)), align 32
  store i256 128, ptr addrspace(5) inttoptr (i256 96 to ptr addrspace(5)), align 32
  store i256 128, ptr addrspace(5) inttoptr (i256 128 to ptr addrspace(5)), align 32
  store i256 128, ptr addrspace(5) inttoptr (i256 160 to ptr addrspace(5)), align 32
  store i256 128, ptr addrspace(5) inttoptr (i256 192 to ptr addrspace(5)), align 32
  store i256 128, ptr addrspace(5) inttoptr (i256 224 to ptr addrspace(5)), align 32
  store i256 128, ptr addrspace(5) inttoptr (i256 256 to ptr addrspace(5)), align 32
  store i256 128, ptr addrspace(5) inttoptr (i256 288 to ptr addrspace(5)), align 32
  ret void
}

; CHECK-LABEL: define void @caller_noinline1
define void @caller_noinline1() {
entry:
; CHECK: call void @callee_noinline()
  call void @callee_noinline()
  call void @llvm.evm.return(ptr addrspace(1) null, i256 0)
  unreachable
}

; CHECK-LABEL: define void @caller_noinline2
define void @caller_noinline2() {
entry:
; CHECK: call void @callee_noinline()
  call void @callee_noinline()
  call void @llvm.evm.return(ptr addrspace(1) null, i256 0)
  unreachable
}
