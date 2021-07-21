; RUN: llc < %s | FileCheck %s

target triple = "syncvm"
target datalayout = "e-p:256:256-i256:256:256"

; CHECK-LABEL: test.selectcc
define i256 @test.selectcc(i256 %v1, i256 %v2, i256 %v3) {
; CHECK:   sfll #34, r5, r5
; CHECK:   sflh #0, r5, r5
; CHECK:   sub r3, r5, r0
; CHECK:   je .LBB0_2, .LBB0_1
; CHECK: .LBB0_1:
; CHECK    add r2, r0, r4
; CHECK: .LBB0_2:
; CHECK    add r4, r0, r1
; CHECK:   ret
  %1 = icmp eq i256 %v3, 34
  %2 = select i1 %1, i256 %v1, i256 %v2
  ret i256 %2
}

; CHECK-LABEL: test.select1
define fastcc i256 @test.select1(i256 %0) {
; CHECK:   sfll #1, r3, r3
; CHECK:   sflh #0, r3, r3
; CHECK:   sfll #10, r2, r2
; CHECK:   sflh #0, r2, r2
; CHECK:   sub r1, r3, r0
; CHECK:   je .LBB1_2, .LBB1_1
; CHECK: .LBB1_1:
; CHECK:   sfll #0, r2, r2
; CHECK:   sflh #0, r2, r2
; CHECK: .LBB1_2:
; CHECK    add r2, r0, r1
; CHECK:   ret
entry:
  %1 = icmp eq i256 %0, 1
  br label %lbl

lbl:            ; preds = %entry
  %.mux.le = select i1 %1, i256 10, i256 0
  ret i256 %.mux.le
}
