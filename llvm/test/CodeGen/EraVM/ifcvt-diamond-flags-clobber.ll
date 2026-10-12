; RUN: llc -O3 -verify-machineinstrs < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; Check that if-conversion does not hoist the common head of a diamond if it
; writes $flags. Here both arms start with umin (sub.s! + add.ge), and hoisting
; it would clobber the condition of the predicated tails.
; CHECK-LABEL: test:
; CHECK:       sub! r1, r0, r0
; CHECK-NEXT:  jump.eq @.BB0_1
; CHECK-NEXT:  ; %bb.2:
; CHECK-NEXT:  sub.s! 1000, r2, r0
; CHECK-NEXT:  add.ge 1000, r0, r2
; CHECK-NEXT:  add 9, r2, r1
; CHECK-NEXT:  ret
; CHECK-NEXT:  .BB0_1:
; CHECK-NEXT:  sub.s! 1000, r2, r0
; CHECK-NEXT:  add.ge 1000, r0, r2
; CHECK-NEXT:  add 7, r2, r1
; CHECK-NEXT:  ret
define i256 @test(i256 %a, i256 %b) optsize {
entry:
  %c = icmp eq i256 %a, 0
  br i1 %c, label %t, label %e
t:
  %m1 = call i256 @llvm.umin.i256(i256 %b, i256 1000)
  %r1 = add i256 %m1, 7
  ret i256 %r1
e:
  %m2 = call i256 @llvm.umin.i256(i256 %b, i256 1000)
  %r2 = add i256 %m2, 9
  ret i256 %r2
}

declare i256 @llvm.umin.i256(i256, i256)
