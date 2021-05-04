; RUN: llc < %s | FileCheck %s

target triple = "syncvm"
target datalayout = "e-p:256:256-i256:256:256"

; CHECK-LABEL: selectcc
define i256 @selectcc(i256 %v1, i256 %v2, i256 %v3) {
; CHECK:   sfll #34, r4, r4
; CHECK:   sflh #0, r4, r4
; CHECK:   sub r3, r4, r0
; CHECK:   je .LBB0_2, .LBB0_1
; CHECK: .LBB0_1:
; CHECK    add r2, r0, r1
; CHECK:   j .LBB0_2, .LBB0_2
; CHECK: .LBB0_2:
; CHECK:   ret
  %1 = icmp eq i256 %v3, 34
  %2 = select i1 %1, i256 %v1, i256 %v2
  ret i256 %2
}
