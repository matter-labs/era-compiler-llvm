; XFAIL: target=evm{{.*}}
; TODO: CPR-918 Lower trap to panic
; RUN: llc < %s
; UNSUPPORTED: target=evm{{.*}}

define i32 @test() noreturn nounwind  {
entry:
	tail call void @llvm.trap( )
	unreachable
}

declare void @llvm.trap() nounwind 

