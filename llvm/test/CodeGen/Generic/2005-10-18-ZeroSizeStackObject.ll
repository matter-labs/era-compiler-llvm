; RUN: llc < %s
; UNSUPPORTED: target=evm{{.*}}

define void @test() {
        %X = alloca {  }                ; <ptr> [#uses=0]
        ret void
}
