; RUN: llc  < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i256 @foo(i256 %a1, i256 %a2, i256 %a3) nounwind {
; CHECK-LABEL: @foo
; CHECK: JUMPDEST
; CHECK-NEXT: SWAP2
; CHECK-NEXT: POP
; CHECK-NEXT: POP
; CHECK-NEXT: DUP1
; CHECK-NEXT: ADD
; CHECK-NEXT: SWAP1
; CHECK-NEXT: JUMP

  %x1 = add i256 %a1, %a1
  ret i256 %x1
}

define i256 @wat(i256 %a1, i256 %a2, i256 %a3) nounwind {
; CHECK-LABEL: @wat
; CHECK: JUMPDEST
; CHECK-NEXT: POP
; CHECK-NEXT: SWAP1
; CHECK-NEXT: POP
; CHECK-NEXT: DUP1
; CHECK-NEXT: ADD
; CHECK-NEXT: SWAP1
; CHECK-NEXT: JUMP

  %x1 = add i256 %a2, %a2
  ret i256 %x1
}

define i256 @bar() nounwind {
; CHECK-LABEL: @bar
; CHECK: JUMPDEST
; CHECK-NEXT: PUSH4 @.FUNC_RET0
; CHECK-NEXT: PUSH1 3
; CHECK-NEXT: PUSH1 2
; CHECK-NEXT: PUSH1 1
; CHECK-NEXT: PUSH4 @foo
; CHECK-NEXT: JUMP
; CHECK-LABEL: .FUNC_RET0:
; CHECK-NEXT: JUMPDEST
; CHECK-NEXT: SWAP1
; CHECK-NEXT: JUMP

  %res = call i256 @foo(i256 1, i256 2, i256 3)
  ret i256 %res
}
