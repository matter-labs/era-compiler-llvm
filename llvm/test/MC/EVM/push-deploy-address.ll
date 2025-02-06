; RUN: llc -O2 -filetype=obj --mtriple=evm %s -o - | llvm-objdump --no-leading-addr --disassemble - | FileCheck %s

; CHECK-LABEL: <test>:
; CHECK-NEXT:  73 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00	PUSH20 0

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

declare i256 @llvm.evm.calldatasize()
declare i256 @llvm.evm.pushdeployaddress()
declare i256 @llvm.evm.address()
declare void @llvm.evm.revert(ptr addrspace(1), i256)
declare void @llvm.evm.return(ptr addrspace(1), i256)

define void @test() noreturn {
entry:
  store i256 128, ptr addrspace(1) inttoptr (i256 64 to ptr addrspace(1)), align 64
  %curr_addr = tail call i256 @llvm.evm.address()
  %calldatasize = tail call i256 @llvm.evm.calldatasize()
  %cmp_calldata = icmp ult i256 %calldatasize, 4
  br i1 %cmp_calldata, label %error, label %check_deploy

check_deploy:
  %deploy_addr = tail call i256 @llvm.evm.pushdeployaddress()
  %cmp = icmp eq i256 %deploy_addr, %curr_addr
  br i1 %cmp, label %exit, label %error

error:
  tail call void @llvm.evm.revert(ptr addrspace(1) noalias nocapture nofree noundef nonnull align 32 null, i256 0)
  unreachable

exit:
  tail call void @llvm.evm.return(ptr addrspace(1) noalias nocapture nofree noundef nonnull align 32 null, i256 0)
  unreachable
}

!1 = !{!"imm_id"}
