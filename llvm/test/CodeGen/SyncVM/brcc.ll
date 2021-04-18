; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: ugt
define i256 @ugt(i256 %p1, i256 %p2) nounwind {
; CHECK: sub r1, r2, r0
; CHECK: jle .LBB0_2, .LBB0_1
  %1 = icmp ugt i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: uge
define i256 @uge(i256 %p1, i256 %p2) nounwind {
; CHECK: sub r1, r2, r0
; CHECK: jlt .LBB1_2, .LBB1_1
  %1 = icmp uge i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: ult
define i256 @ult(i256 %p1, i256 %p2) nounwind {
; CHECK: sub r1, r2, r0
; CHECK: jge .LBB2_2, .LBB2_1
  %1 = icmp ult i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: ule
define i256 @ule(i256 %p1, i256 %p2) nounwind {
; CHECK: sub r1, r2, r0
; CHECK: jgt .LBB3_2, .LBB3_1
  %1 = icmp ule i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: eq
define i256 @eq(i256 %p1, i256 %p2) nounwind {
; CHECK: sub r1, r2, r0
; CHECK-NEXT: jne .LBB4_2, .LBB4_1
  %1 = icmp eq i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: cmpne
define i256 @cmpne(i256 %p1, i256 %p2) nounwind {
; CHECK: sub r1, r2, r0
; CHECK: je .LBB5_2, .LBB5_1
  %1 = icmp ne i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

define i256 @loop(i256 %p1) {
entry:
  br label %loop.cond
loop.cond:
  %i = phi i256 [0, %entry], [%i.next, %loop.body]
  %res = phi i256 [0, %entry], [%res.next, %loop.body]
  %cond = icmp ne i256 %i, %p1
  br i1 %cond, label %loop.body, label %loop.exit
loop.body:
  %i.next = add i256 %i, 1
  %res.next = add i256 %res, %i
  br label %loop.cond
loop.exit:
  ret i256 %res
}
