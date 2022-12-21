; RUN: llc < %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

declare ptr addrspace(3) @llvm.eravm.ptr.shrink(ptr addrspace(3), i256)

define i256 @__entry() {
  call void @__runtime()
  unreachable
}

define private ptr addrspace(3) @__runtime() {
entry:
  %active_pointer_shrunken = call align 32 ptr addrspace(3) @llvm.eravm.ptr.shrink(ptr addrspace(3) align 32 undef, i256 65535)
  ret ptr addrspace(3) %active_pointer_shrunken
}
