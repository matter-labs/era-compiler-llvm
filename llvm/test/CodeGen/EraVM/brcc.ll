; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: ugt
define i256 @ugt(i256 %p1, i256 %p2) nounwind {
; CHECK: sub! r1, r2, r{{[0-9]+}}
; CHECK-NEXT: add.le 72, r0, r1
; CHECK-NEXT: add.gt  42, r0, r1
; CHECK-NEXT: ret
  %1 = icmp ugt i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: uge
define i256 @uge(i256 %p1, i256 %p2) nounwind {
; CHECK: sub! r1, r2, r{{[0-9]+}}
; CHECK-NEXT: add.lt 72, r0, r1
; CHECK-NEXT: add.ge 42, r0, r1
; CHECK-NEXT: ret
  %1 = icmp uge i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: ult
define i256 @ult(i256 %p1, i256 %p2) nounwind {
; CHECK: sub! r1, r2, r{{[0-9]+}}
; CHECK-NEXT: add.ge 72, r0, r1
; CHECK-NEXT: add.lt  42, r0, r1
; CHECK-NEXT: ret
  %1 = icmp ult i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: ule
define i256 @ule(i256 %p1, i256 %p2) nounwind {
; CHECK: sub! r1, r2, r{{[0-9]+}}
; CHECK-NEXT: add.gt  72, r0, r1
; CHECK-NEXT: add.le  42, r0, r1
; CHECK-NEXT: ret
  %1 = icmp ule i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: eq
define i256 @eq(i256 %p1, i256 %p2) nounwind {
; CHECK: sub! r1, r2, r{{[0-9]+}}
; CHECK-NEXT: add.ne  72, r0, r1
; CHECK-NEXT: add.eq  42, r0, r1
; CHECK-NEXT: ret
  %1 = icmp eq i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: cmpne
define i256 @cmpne(i256 %p1, i256 %p2) nounwind {
; CHECK: sub! r1, r2, r{{[0-9]+}}
; CHECK-NEXT: add.eq  72, r0, r1
; CHECK-NEXT: add.ne  42, r0, r1
; CHECK-NEXT: ret
  %1 = icmp ne i256 %p1, %p2
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: loop
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

; CHECK-LABEL: cmpir
define i256 @cmpir(i256 %p1, i256 %p2) nounwind {
; CHECK: sub.s! 43, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: add.lt  72, r0, r1
; CHECK-NEXT: add.ge  42, r0, r1
; CHECK-NEXT: ret
  %1 = icmp ugt i256 %p1, 42
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: cmpcr
define i256 @cmpcr(i256 %p1, i256 %p2) nounwind {
; CHECK: sub.s! @val[0], r1, r{{[0-9]+}}
; CHECK-NEXT: add.le  72, r0, r1
; CHECK-NEXT: add.gt  42, r0, r1
; CHECK-NEXT: ret
  %const = load i256, i256 addrspace(4)* @val
  %1 = icmp ugt i256 %p1, %const
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: cmpsr
define i256 @cmpsr(i256 %p1, i256 %p2) nounwind {
  %ptr = alloca i256
; CHECK: sub.s! stack-[1], r1, r1
  %data = load i256, i256* %ptr
  %1 = icmp ugt i256 %p1, %data
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: cmpri
define i256 @cmpri(i256 %p1, i256 %p2) nounwind {
; CHECK: sub.s! 41, r1, r1
; CHECK-NEXT: add.gt  72, r0, r1
; CHECK-NEXT: add.le  42, r0, r1
; CHECK-NEXT: ret
  %1 = icmp ugt i256 42, %p1
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: cmprc
define i256 @cmprc(i256 %p1, i256 %p2) nounwind {
; CHECK: sub! @val[0], r1, r{{[0-9]+}}
; CHECK-NEXT: add.le  72, r0, r1
; CHECK-NEXT: add.gt  42, r0, r1
; CHECK-NEXT: ret
  %const = load i256, i256 addrspace(4)* @val
  %1 = icmp ugt i256 %const, %p1
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}

; CHECK-LABEL: cmprs
define i256 @cmprs(i256 %p1, i256 %p2) nounwind {
  %ptr = alloca i256
; CHECK: sub! stack-[1], r1, r{{[0-9]+}}
  %data = load i256, i256* %ptr
  %1 = icmp ugt i256 %data, %p1
  br i1 %1, label %l1, label %l2
l1:
  ret i256 42
l2:
  ret i256 72
}
