; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm-unknown-unknown"

@ptr_calldata = private global ptr addrspace(3) null

; Function Attrs: argmemonly nocallback nofree nounwind willreturn
declare void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(3) noalias nocapture readonly, i256, i1 immarg) #5

; CHECK-LABEL: foo
define void @foo() {
entry:
  %calldata_pointer = load ptr addrspace(3), ptr @ptr_calldata, align 32
  %calldata_source_pointer = getelementptr i8, ptr addrspace(3) %calldata_pointer, i256 122
; CHECK: ld.inc  r2, r4, r2
; CHECK: st.1.inc        r1, r4, r1
  call void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) align 1 inttoptr (i256 128 to ptr addrspace(1)), ptr addrspace(3) align 1 %calldata_source_pointer, i256 64, i1 false)
  unreachable

return:                                           ; No predecessors!
  ret void
}

