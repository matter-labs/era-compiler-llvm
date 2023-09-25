; RUN: llc < %s
target datalayout = "E-p:256:256-i256:256:256-S32"
target triple = "eravm"

; CHECK-LABEL: lower_trap
define i32 @lower_trap() noreturn nounwind  {
entry:
; CHECK: ret.panic r0
	tail call void @llvm.trap( )
	unreachable
}

declare void @llvm.trap() nounwind 

