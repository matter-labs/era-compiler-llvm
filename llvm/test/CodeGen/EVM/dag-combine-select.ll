; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

; Function for cond ? X : 0 (false value is 0)
define i256 @sel_x_or_0(i1 %cond, i256 %x) {
%val = select i1 %cond, i256 %x, i256 0
ret i256 %val
}
; CHECK-LABEL: @sel_x_or_0
; CHECK: MUL
; CHECK-NOT: JUMPI

; Function for cond ? 0 : Y (true value is 0)
define i256 @sel_0_or_y(i1 %cond, i256 %y) {
%val = select i1 %cond, i256 0, i256 %y
ret i256 %val
}
; CHECK-LABEL: @sel_0_or_y
; CHECK:      ISZERO
; CHECK-NEXT: MUL
; CHECK-NOT: JUMPI
