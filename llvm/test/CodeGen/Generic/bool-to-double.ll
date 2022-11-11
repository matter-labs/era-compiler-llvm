; RUN: llc < %s
; EVM doesn't support floats
; UNSUPPORTED: target=evm{{.*}}
define double @test(i1 %X) {
        %Y = uitofp i1 %X to double             ; <double> [#uses=1]
        ret double %Y
}

