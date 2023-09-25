; UNSUPPORTED: eravm
; EraVM doesn't support inline asm yet.
; RUN: llc -no-integrated-as < %s | FileCheck %s

define void @bar() nounwind {
  ; CHECK: foo 0 0{{$}}
  tail call void asm sideeffect "foo ${:uid} ${:uid}", ""() nounwind
  ; CHECK: bar 1 x{{$}}
  tail call void asm sideeffect "bar $(${:uid} x$| ${:uid} x$)", ""() nounwind
  ret void
}
