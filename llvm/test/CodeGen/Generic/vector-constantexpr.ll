; UNSUPPORTED: target=eravm{{.*}}, target=evm{{.*}}
; EraVM doesn't support vector instructions.
; RUN: llc < %s
	
define void @""(ptr %inregs, ptr %outregs) {
        %a_addr.i = alloca <4 x float>          ; <ptr> [#uses=1]
        store <4 x float> < float undef, float undef, float undef, float undef >, ptr %a_addr.i
        ret void
}
