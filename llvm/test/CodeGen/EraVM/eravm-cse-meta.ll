; RUN: opt -passes=eravm-cse -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i1 @test_dce() {
; CHECK-LABEL: @test_dce(
; CHECK-NOT:     call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  %val2 = call i256 @llvm.eravm.meta()
  %cmp = icmp eq i256 %val1, %val2
  ret i1 %cmp
}

define i256 @test_noelim1() {
; CHECK-LABEL: @test_noelim1(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  %load = load i256, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 64
  %val2 = call i256 @llvm.eravm.meta()
  %sum = add i256 %val1, %val2
  %res = add i256 %sum, %load
  ret i256 %res
}

define i256 @test_noelim2() {
; CHECK-LABEL: @test_noelim2(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  %load = load i256, ptr addrspace(1) inttoptr (i256 1 to ptr addrspace(1)), align 64
  %val2 = call i256 @llvm.eravm.meta()
  %sum = add i256 %val1, %val2
  %res = add i256 %sum, %load
  ret i256 %res
}

define i256 @test_noelim3() {
; CHECK-LABEL: @test_noelim3(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  store i256 2, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 64
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim4() {
; CHECK-LABEL: @test_noelim4(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  store i256 1, ptr addrspace(1) inttoptr (i256 1 to ptr addrspace(1)), align 64
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim5() {
; CHECK-LABEL: @test_noelim5(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) inttoptr (i256 96 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 256 to ptr addrspace(1)), i256 53, i1 false)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim6() {
; CHECK-LABEL: @test_noelim6(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call void @llvm.memcpy.p2.p2.i256(ptr addrspace(2) inttoptr (i256 96 to ptr addrspace(2)), ptr addrspace(2) inttoptr (i256 256 to ptr addrspace(2)), i256 53, i1 false)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim7() {
; CHECK-LABEL: @test_noelim7(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call void @llvm.memcpy.p2.p5.i256(ptr addrspace(2) inttoptr (i256 96 to ptr addrspace(2)), ptr addrspace(5) inttoptr (i256 256 to ptr addrspace(5)), i256 53, i1 false)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim8() {
; CHECK-LABEL: @test_noelim8(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 96 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 256 to ptr addrspace(1)), i256 53, i1 false)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim9() {
; CHECK-LABEL: @test_noelim9(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call void @llvm.memmove.p2.p2.i256(ptr addrspace(2) inttoptr (i256 96 to ptr addrspace(2)), ptr addrspace(2) inttoptr (i256 256 to ptr addrspace(2)), i256 53, i1 false)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim10() {
; CHECK-LABEL: @test_noelim10(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call void @llvm.memset.p1.i256(ptr addrspace(1) inttoptr (i256 96 to ptr addrspace(1)), i8 0, i256 53, i1 false)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim11() {
; CHECK-LABEL: @test_noelim11(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call void @llvm.memset.p2.i256(ptr addrspace(2) inttoptr (i256 96 to ptr addrspace(2)), i8 0, i256 53, i1 false)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim12() {
; CHECK-LABEL: @test_noelim12(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call i256 @llvm.eravm.precompile(i256 0, i256 0)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim13() {
; CHECK-LABEL: @test_noelim13(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call i8 addrspace(3)* @llvm.eravm.decommit(i256 0, i256 0)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim14() {
; CHECK-LABEL: @test_noelim14(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call void @dummy()
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim15(i1 %cond) {
; CHECK-LABEL: @test_noelim15(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @llvm.eravm.meta()
; CHECK:       ret:
; CHECK-NEXT:    call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  br i1 %cond, label %then, label %else

then:
  call void @dummy()
  br label %ret

else:
  br label %ret

ret:
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim16(i256 %loop_size, i1 %cond) {
; CHECK:       entry:
; CHECK-NEXT:    call i256 @llvm.eravm.meta()
; CHECK:       loop:
; CHECK:         call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  br i1 %cond, label %loop, label %ret

loop:
  %phi1 = phi i256 [ %sum, %loop ], [ 0, %entry ]
  %val2 = call i256 @llvm.eravm.meta()
  call void @dummy()
  %sum = add nuw nsw i256 %phi1, %val2
  %cmp = icmp slt i256 %sum, %loop_size
  br i1 %cmp, label %loop, label %ret

ret:
  %phi2 = phi i256 [ %val1, %entry ], [ %sum, %loop ]
  ret i256 %phi2
}

define i256 @test_elim1() {
; CHECK-LABEL: @test_elim1(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK-NOT:     call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  %load = load i256, ptr addrspace(5) inttoptr (i256 5 to ptr addrspace(5)), align 64
  %val2 = call i256 @llvm.eravm.meta()
  %sum = add i256 %val1, %val2
  %res = add i256 %sum, %load
  ret i256 %res
}

define i256 @test_elim2() {
; CHECK-LABEL: @test_elim2(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK-NOT:     call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  store i256 5, ptr addrspace(5) inttoptr (i256 5 to ptr addrspace(5)), align 64
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_elim3() {
; CHECK-LABEL: @test_elim3(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK-NOT:     call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call void @llvm.memcpy.p5.p5.i256(ptr addrspace(5) inttoptr (i256 96 to ptr addrspace(5)), ptr addrspace(5) inttoptr (i256 256 to ptr addrspace(5)), i256 53, i1 false)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_elim4() {
; CHECK-LABEL: @test_elim4(
; CHECK:         call i256 @llvm.eravm.meta()
; CHECK-NOT:     call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  call void @llvm.memset.p5.i256(ptr addrspace(5) inttoptr (i256 96 to ptr addrspace(5)), i8 0, i256 53, i1 false)
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_elim5(i1 %cond) {
; CHECK-LABEL: @test_elim5(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @llvm.eravm.meta()
; CHECK:       ret:
; CHECK-NOT:     call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  br i1 %cond, label %then, label %else

then:
  br label %ret

else:
  br label %ret

ret:
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_elim6(i1 %cond) {
; CHECK-LABEL: @test_elim6(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @llvm.eravm.meta()
; CHECK:       ret1:
; CHECK-NEXT:    call i256 @llvm.eravm.meta()
; CHECK-NOT:     call i256 @llvm.eravm.meta()
; CHECK:       ret2:
; CHECK-NOT:     call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  br i1 %cond, label %check, label %ret2

check:
  store i256 2, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 64
  br label %ret1

ret1:
  %val2 = call i256 @llvm.eravm.meta()
  %val3 = call i256 @llvm.eravm.meta()
  %sum1 = add i256 %val2, %val3
  %res1 = add i256 %val1, %sum1
  ret i256 %res1

ret2:
  %val4 = call i256 @llvm.eravm.meta()
  %res2 = add i256 %val1, %val4
  ret i256 %res2
}

define i256 @test_elim7(i1 %cond) {
; CHECK-LABEL: @test_elim7(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @llvm.eravm.meta()
; CHECK:       ret1:
; CHECK-NOT:     call i256 @llvm.eravm.meta()
; CHECK-NOT:     call i256 @llvm.eravm.meta()
; CHECK:       ret2:
; CHECK-NOT:     call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  br i1 %cond, label %check, label %ret2

check:
  store i256 5, ptr addrspace(5) inttoptr (i256 5 to ptr addrspace(5)), align 64
  br label %ret1

ret1:
  %val2 = call i256 @llvm.eravm.meta()
  %val3 = call i256 @llvm.eravm.meta()
  %sum1 = add i256 %val2, %val3
  %res1 = add i256 %val1, %sum1
  ret i256 %res1

ret2:
  %val4 = call i256 @llvm.eravm.meta()
  %res2 = add i256 %val1, %val4
  ret i256 %res2
}

define i256 @test_elim8(i1 %cond1, i1 %cond2) {
; CHECK-LABEL: @test_elim8(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @llvm.eravm.meta()
; CHECK:       ret2:
; CHECK-NOT:     call i256 @llvm.eravm.meta()
;
entry:
  %val1 = call i256 @llvm.eravm.meta()
  br i1 %cond1, label %then, label %else

then:
  br label %ret2

else:
  br i1 %cond2, label %ret2, label %ret1

ret1:
  store i256 2, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 64
  ret i256 0

ret2:
  %val2 = call i256 @llvm.eravm.meta()
  %res = add i256 %val1, %val2
  ret i256 %res
}

declare void @dummy()
declare i256 @llvm.eravm.meta()
declare i256 @llvm.eravm.precompile(i256, i256)
declare i8 addrspace(3)* @llvm.eravm.decommit(i256, i256)
declare void @llvm.memcpy.p1.p1.i256(ptr addrspace(1), ptr addrspace(1), i256, i1)
declare void @llvm.memcpy.p2.p2.i256(ptr addrspace(2), ptr addrspace(2), i256, i1)
declare void @llvm.memcpy.p2.p5.i256(ptr addrspace(2), ptr addrspace(5), i256, i1)
declare void @llvm.memcpy.p5.p5.i256(ptr addrspace(5), ptr addrspace(5), i256, i1)
declare void @llvm.memmove.p1.p1.i256(ptr addrspace(1), ptr addrspace(1), i256, i1)
declare void @llvm.memmove.p2.p2.i256(ptr addrspace(2), ptr addrspace(2), i256, i1)
declare void @llvm.memset.p1.i256(ptr addrspace(1), i8, i256, i1)
declare void @llvm.memset.p2.i256(ptr addrspace(2), i8, i256, i1)
declare void @llvm.memset.p5.i256(ptr addrspace(5), i8, i256, i1)
