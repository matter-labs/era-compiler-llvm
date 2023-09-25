; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; CHECK-LABEL: test1
define i256 @test1(i256 %x, i1 %y) {
entry:
  %x1 = add i256 %x, 1
  br i1 %y, label %BB1, label %BB2
; CHECK: sub! r2, r0,
; CHECK-NEXT: add.ne 1, r1,
; CHECK-NEXT: ; %bb.1:
; CHECK-NEXT: ret
BB1:                                             
  %z1 = add i256 %x1, 1
  br label %BB2
BB2:                                             
  %z = phi i256 [%z1, %BB1], [%x1, %entry]  
  ret i256 %z
}

; CHECK-LABEL: test2
define i256 @test2(i256 %x, i1 %y) {
entry:
  %x1 = add i256 %x, 1
  br i1 %y, label %BB2, label %BB1
; CHECK: sub! r2, r0
; CHECK-NEXT: add.eq 1, r1
; CHECK-NEXT: %bb.1:
; CHECK-NEXT: ret
BB1:                                             
  %z1 = add i256 %x1, 1
  br label %BB2
BB2:                                             
  %z = phi i256 [%z1, %BB1], [%x1, %entry]  
  ret i256 %z
}

; CHECK-LABEL: test3
define i256 @test3(i256 %x, i1 %y) {
entry:
  %x1 = add i256 %x, 1
  br i1 %y, label %BB2, label %BB1

; TODO CPR-1223
; a more optimal code sequence is more desirable here
; (same as in test4) but would require manipulating the
; block sequence.

; CHECK: sub! r2, r0
; CHECK-NEXT: jump.ne @.BB2_1
; CHECK-NEXT: %bb.2:
; CHECK-NEXT: add 1, r1, r1
BB2:                                             
  %z = phi i256 [%z1, %BB1], [%x1, %entry]  
  ret i256 %z
BB1:                                             
  %z1 = add i256 %x1, 1
  br label %BB2
}

; CHECK-LABEL: test4
define i256 @test4(i256 %x, i1 %y) {
entry:
  %x1 = add i256 %x, 1
  br i1 %y, label %BB1, label %BB2
; TODO: CPR-1223 eliminate the jump
; CHECK: sub! r2, r0
; CHECK-NEXT: jump.eq @.BB3_1
; CHECK-NEXT: %bb.2:
; CHECK-NEXT: add     1, r1
BB2:                                             
  %z = phi i256 [%z1, %BB1], [%x1, %entry]  
  ret i256 %z
BB1:                                             
  %z1 = add i256 %x1, 1
  br label %BB2
}

; checks that we do not if-convert a branch if it is not profitable. 
; CHECK-LABEL: test5
define i256 @test5(i256 %x, i1 %y) {
entry:
  %x1 = add i256 %x, 1
  %x2 = add i256 %x, 1024
  %x3 = add i256 %x, 1017
  %x4 = add i256 %x, 1323
  br i1 %y, label %BB1, label %BB2
; CHECK: sub! r2, r0,
; CHECK-NOT: add.ne 1, r1,
; CHECK-NOT: add.ne 1024,
; CHECK-NOT: add.ne 1017, 
BB1:                                             
  %y1 = add i256 %x1, 1
  %y2 = add i256 %x2, 1024
  %y3 = add i256 %x3, 1017
  %y4 = add i256 %x4, 134
  br label %BB2
BB2:                                             
  %z1 = phi i256 [%y1, %BB1], [%x1, %entry]  
  %z2 = phi i256 [%y2, %BB1], [%x2, %entry]
  %z3 = phi i256 [%y3, %BB1], [%x3, %entry]
  %z4 = phi i256 [%y4, %BB1], [%x4, %entry]

  %z5 = mul i256 %z1, %z2
  %z6 = mul i256 %z4, %z3
  %z7 = mul i256 %z5, %z6
  ret i256 %z7
}

; checks that we do not if-convert diamond if it is not profitable. 
; CHECK-LABEL: test6
define i256 @test6(i256 %x, i256* %ptr.i256) {
entry:
  %x1 = add i256 %x, 1
  %x2 = add i256 %x, 1024
  %x3 = add i256 %x, 1017
  %y = icmp eq i256* %ptr.i256, null
  br i1 %y, label %BB1, label %BB2
; CHECK: sub! r2, r0,
; CHECK-NOT: add.ne 1, r1,
; CHECK-NOT: add.ne 1024,
; CHECK-NOT: add.ne 1017, 
BB1:                                             
  %y1 = add i256 %x1, 1
  %y2 = add i256 %x2, 1024
  %y3 = add i256 %x3, 1017
  br label %BB3
BB2:                                             
  %yy1 = add i256 %x1, 13 
  %yy2 = add i256 %x2, 14
  %yy3 = add i256 %x3, 17 
  br label %BB3
BB3:
  %z1 = phi i256 [%y1, %BB1], [%yy1, %BB2]
  %z2 = phi i256 [%y2, %BB1], [%yy2, %BB2]
  %z3 = phi i256 [%y3, %BB1], [%yy3, %BB2]

  %z4 = mul i256 %z1, %z2
  %z5 = mul i256 %z4, %z3
  ret i256 %z5
}
