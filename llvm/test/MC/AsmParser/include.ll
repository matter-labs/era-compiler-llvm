; RUN: llc -I %p/Inputs -filetype asm -o - %s | FileCheck %s
; UNSUPPORTED: target={{.*}}-zos{{.*}},target=nvptx{{.*}}
; REQUIRES: default_triple

; EraVM/EVM doesn't support inline asm (yet).
; XFAIL: target=eravm{{.*}}, target=evm{{.*}}

module asm ".include \22module.x\22"

define void @f() {
entry:
  call void asm sideeffect ".include \22function.x\22", ""()
  ret void
}

; CHECK: .set MODULE, 1
; CHECK: .set FUNCTION, 1
