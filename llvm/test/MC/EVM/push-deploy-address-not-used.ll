; RUN: llc -O2 -filetype=obj --mtriple=evm %s -o - | llvm-objdump --no-leading-addr --disassemble - | FileCheck %s

; CHECK-LABEL: <test>:
; CHECK-NEXT:  73 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00	PUSH20 0

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

declare i256 @llvm.evm.pushdeployaddress()
declare i256 @llvm.evm.address()
declare void @llvm.evm.revert.sptr(ptr addrspace(1), i256, ptr addrspace(5), ptr addrspace(6))
declare void @llvm.evm.return.sptr(ptr addrspace(1), i256, ptr addrspace(5), ptr addrspace(6))

define void @test() noreturn {
entry:
  store i256 128, ptr addrspace(1) inttoptr (i256 64 to ptr addrspace(1)), align 64
  %deploy_addr = tail call i256 @llvm.evm.pushdeployaddress()
  %curr_addr = tail call i256 @llvm.evm.address()
  %cmp = icmp eq i256 %curr_addr, 0
  br i1 %cmp, label %exit, label %error

error:
  tail call void @llvm.evm.revert.sptr(ptr addrspace(1) noalias nocapture nofree noundef nonnull align 32 null, i256 0, ptr addrspace(5) null, ptr addrspace(6) null)
  unreachable

exit:
  tail call void @llvm.evm.return.sptr(ptr addrspace(1) noalias nocapture nofree noundef nonnull align 32 null, i256 0, ptr addrspace(5) null, ptr addrspace(6) null)
  unreachable
}

!1 = !{!"imm_id"}
