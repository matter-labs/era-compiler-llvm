; RUN: opt -S -memcpyopt < %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

@ptr_calldata = private global ptr addrspace(3) null

declare void @foo()

; Function Attrs: argmemonly nocallback nofree nounwind willreturn
declare void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(1) noalias nocapture readonly, i256, i1 immarg)

; Function Attrs: argmemonly nocallback nofree nounwind willreturn
declare void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(3) noalias nocapture readonly, i256, i1 immarg)

; Function Attrs: nofree null_pointer_is_valid
define private void @__deploy() {
entry:
  call void @foo()
  %calldata_pointer = load ptr addrspace(3), ptr @ptr_calldata, align 32
  %calldata_source_pointer = getelementptr i8, ptr addrspace(3) %calldata_pointer, i256 0
  call void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) align 1 null, ptr addrspace(3) align 1 %calldata_source_pointer, i256 32000000000000000000000000000000000000000000, i1 false)
  unreachable
}
