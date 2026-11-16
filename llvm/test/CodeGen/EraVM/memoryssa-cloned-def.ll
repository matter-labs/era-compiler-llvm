; RUN: opt -disable-output -passes="loop-mssa(simple-loop-unswitch<nontrivial>),print<memoryssa>" -verify-memoryssa < %s 2>&1 | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

declare i256 @llvm.eravm.gasleft() memory(inaccessiblemem: readwrite)

; CHECK-LABEL: MemorySSA for function: test_unswitch_removed_def_i256

; CHECK: entry:
; CHECK-NEXT: 1 = MemoryDef(liveOnEntry)
; CHECK-NEXT: store i256 10, ptr addrspace(1) %p, align 32

; CHECK: for.body.us:
; CHECK-NEXT: 5 = MemoryPhi({entry.split.us,1},{for.body.us,6})
; CHECK-NEXT: 6 = MemoryDef(5)
; CHECK-NEXT: store i256 20, ptr addrspace(1) %p, align 32

; CHECK: for.body:
; CHECK-NEXT: 4 = MemoryPhi({entry.split,1},{for.body,3})
; CHECK-NEXT: 2 = MemoryDef(4)
; CHECK-NEXT: store i256 20, ptr addrspace(1) %p, align 32
; CHECK-NEXT: 3 = MemoryDef(2)
; CHECK-NEXT: %g = call i256 @llvm.eravm.gasleft()

define void @test_unswitch_removed_def_i256(ptr addrspace(1) %p, i1 %arg) {
entry:
  store i256 10, ptr addrspace(1) %p, align 32
  br label %for.body

for.body:
  store i256 20, ptr addrspace(1) %p, align 32
  %g = call i256 @llvm.eravm.gasleft()
  %cmp = icmp ne i256 %g, 0
  %or.cond = select i1 %cmp, i1 %arg, i1 false
  br i1 %or.cond, label %exit, label %for.body

exit:
  ret void
}
