; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare { i256, i1 } @llvm.uadd.with.overflow.i256(i256, i256)
declare { i256, i1 } @llvm.usub.with.overflow.i256(i256, i256)
declare { i256, i1 } @llvm.umul.with.overflow.i256(i256, i256)

; CHECK-LABEL: add_test
define i256 @add_test(i256 %a, i256 %b, i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 %y)
; CHECK:      add!    r3, r4, r{{[0-9]+}}
; CHECK-NEXT: add.ge  r2, r0, r1
  %overflow = extractvalue {i256, i1} %res1, 1
  %selected = select i1 %overflow, i256 %a, i256 %b
  ret i256 %selected
}

; CHECK-LABEL: add_test_2
define void @add_test_2(i256 %a, i256 %b, i256 %c, i256 %d) {
  %resptr = alloca i256
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %c, i256 %d)
  %overflow = extractvalue {i256, i1} %res1, 1
  %val = select i1 %overflow, i256 42, i256 %a
; CHECK: add!    r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add.lt  42, r0, r1
  store i256 %val, i256* %resptr
  ret void
}

; CHECK-LABEL: add_test_3
define i256 @add_test_3(i256 %a, i256 %b, i256 %c, i256 %x, i256 %y, i256 %z) {
entry:
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 %y)
  %val = extractvalue {i256, i1} %res1, 0
  %cmp = icmp uge i256 %a, %b
  %input1 = select i1 %cmp, i256 %val, i256 %c
  %overflow = extractvalue {i256, i1} %res1, 1
  %selected = select i1 %overflow, i256 %a, i256 %b
  %sum = add i256 %input1, %selected
; CHECK:      add!    r4, r5, r6
; CHECK-NEXT: add     r2, r0, r6
; CHECK-NEXT: add.lt  r1, r0, r6
; CHECK-NEXT: add     r4, r5, r4
; CHECK-NEXT: sub!    r1, r2, r1
  ret i256 %sum
}

; CHECK-LABEL: add_test_4
define i256 @add_test_4(i256 %a, i256 %b, i256 %c, i256 %x, i256 %y, i256 %z) {
entry:
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 %y)
  %val = extractvalue {i256, i1} %res1, 0
  %cmp = icmp uge i256 %a, %b
  %input1 = select i1 %cmp, i256 %val, i256 %c
  %overflow = extractvalue {i256, i1} %res1, 1
  %selected = select i1 %overflow, i256 %input1, i256 %b
  %sum = add i256 %z, %selected
; CHECK:      add     r4, r5, r7
; CHECK-NEXT: sub!    r1, r2, r1
; CHECK-NEXT: add.lt  r3, r0, r7
; CHECK-NEXT: add!    r4, r5, r1
; CHECK-NEXT: add.ge  r2, r0, r7
  ret i256 %sum
}

; CHECK-LABEL: sub_test
define i256 @sub_test(i256 %a, i256 %b, i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %x, i256 %y)
; CHECK:      sub!    r3, r4, r{{[0-9]+}}
; CHECK-NEXT: add.ge  r2, r0, r1
  %overflow = extractvalue {i256, i1} %res1, 1
  %selected = select i1 %overflow, i256 %a, i256 %b
  ret i256 %selected
}

; CHECK-LABEL: sub_test_2
define void @sub_test_2(i256 %a, i256 %b, i256 %c, i256 %d) {
  %resptr = alloca i256
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %c, i256 %d)
  %overflow = extractvalue {i256, i1} %res1, 1
  %val = select i1 %overflow, i256 42, i256 %a
; CHECK: sub!    r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add.lt  42, r0, r1
  store i256 %val, i256* %resptr
  ret void
}

; CHECK-LABEL: mul_test
define i256 @mul_test(i256 %a, i256 %b, i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %x, i256 %y)
; CHECK:      mul!    r3, r4, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: add.ge  r2, r0, r1
  %overflow = extractvalue {i256, i1} %res1, 1
  %selected = select i1 %overflow, i256 %a, i256 %b
  ret i256 %selected
}

; CHECK-LABEL: mul_test_2
define void @mul_test_2(i256 %a, i256 %b, i256 %c, i256 %d) {
  %resptr = alloca i256
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %c, i256 %d)
  %overflow = extractvalue {i256, i1} %res1, 1
  %val = select i1 %overflow, i256 42, i256 %a
; CHECK: mul!    r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add.lt  42, r0, r1
  store i256 %val, i256* %resptr
  ret void
}
