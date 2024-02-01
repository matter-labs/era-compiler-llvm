; RUN: llc -O2 -filetype=obj --mtriple=evm %s -o - | llvm-objdump -r - | FileCheck %s

; CHECK: RELOCATION RECORDS FOR [.text]:
; CHECK-NEXT: OFFSET   TYPE        VALUE
; CHECK-NEXT: {{\d*}} R_EVM_DATA   __datasize_D_105_deployed
; CHECK-NEXT: {{\d*}} R_EVM_DATA   __dataoffset_D_105_deployed

; TODO: CRP-1575. Rewrite the test in assembly.
target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

; Function Attrs: nounwind
declare i256 @llvm.evm.callvalue()
declare i256 @llvm.evm.datasize(metadata)
declare i256 @llvm.evm.dataoffset(metadata)
declare i256 @llvm.evm.codesize()

declare void @llvm.memcpy.p1i256.p4i256.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(4) noalias nocapture readonly, i256, i1 immarg)

; Function Attrs: noreturn nounwind
declare void @llvm.evm.return(ptr addrspace(1), i256) #1

; Function Attrs: noreturn nounwind
declare void @llvm.evm.revert(ptr addrspace(1), i256) #1

; Function Attrs: nofree noinline noreturn null_pointer_is_valid
define void @__entry() local_unnamed_addr #2 {
entry:
  store i256 128, ptr addrspace(1) inttoptr (i256 64 to ptr addrspace(1)), align 64
  %callvalue = tail call i256 @llvm.evm.callvalue()
  %if_condition_compared.not = icmp eq i256 %callvalue, 0
  br i1 %if_condition_compared.not, label %if_join, label %if_main

if_main:                                          ; preds = %entry
  tail call void @llvm.evm.revert(ptr addrspace(1) noalias nocapture nofree noundef nonnull align 32 null, i256 0)
  unreachable

if_join:                                         ; preds = %entry
  %deployed_size = tail call i256 @llvm.evm.datasize(metadata !1)
  %deployed_off = tail call i256 @llvm.evm.dataoffset(metadata !1)
  %rt_ptr = inttoptr i256 %deployed_off to ptr addrspace(4)
  call void @llvm.memcpy.p1i256.p4i256.i256(ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), ptr addrspace(4) %rt_ptr, i256 %deployed_size, i1 false)
  tail call void @llvm.evm.return(ptr addrspace(1) noalias nocapture nofree noundef nonnull align 32 inttoptr (i256 128 to ptr addrspace(1)), i256 %deployed_size) 
  unreachable
}

attributes #0 = { nounwind }
attributes #1 = { noreturn nounwind }
attributes #2 = { nofree noinline noreturn null_pointer_is_valid }

!1 = !{!"D_105_deployed"}
