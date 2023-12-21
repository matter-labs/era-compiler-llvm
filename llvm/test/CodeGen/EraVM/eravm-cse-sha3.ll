; RUN: opt -passes=eravm-cse -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i1 @test_dce1(ptr addrspace(1) nocapture %addr) {
; CHECK-LABEL: @test_dce1(
; CHECK-NOT:     call i256 @__sha3
;
entry:
  %val = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  ret i1 true
}

define i1 @test_dce2(ptr addrspace(1) nocapture %addr) {
; CHECK-LABEL: @test_dce2(
; CHECK-NOT:     call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %val2 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %cmp = icmp eq i256 %val1, %val2
  ret i1 %cmp
}

define i256 @test_noelim1(ptr addrspace(1) nocapture %addr) {
; CHECK-LABEL: @test_noelim1(
; CHECK:         call i256 @__sha3
; CHECK:         call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  call void @dummy()
  %val2 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim2(ptr addrspace(1) nocapture %addr, ptr addrspace(1) nocapture %ptr1) {
; CHECK-LABEL: @test_noelim2(
; CHECK:         call i256 @__sha3
; CHECK:         call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  store i256 1, ptr addrspace(1) %ptr1, align 1
  %val2 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim3(ptr addrspace(1) nocapture %addr) {
; CHECK-LABEL: @test_noelim3(
; CHECK:         call i256 @__sha3
; CHECK:         call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  store i256 1, ptr addrspace(1) inttoptr (i256 80 to ptr addrspace(1)), align 1
  %val2 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_noelim4() {
; CHECK-LABEL: @test_noelim4(
; CHECK:         call i256 @__sha3
; CHECK:         call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1)), i256 96, i1 true)
  store i256 1, ptr addrspace(1) inttoptr (i256 80 to ptr addrspace(1)), align 1
  %val2 = call i256 @__sha3(ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1)), i256 96, i1 true)
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_elim1(ptr addrspace(1) nocapture %addr) {
; CHECK-LABEL: @test_elim1(
; CHECK:         call i256 @__sha3
; CHECK-NOT:     call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %val2 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_elim2(ptr addrspace(1) nocapture %addr) {
; CHECK-LABEL: @test_elim2(
; CHECK:         call i256 @__sha3
; CHECK-NOT:     call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  store i256 2, ptr addrspace(2) inttoptr (i256 2 to ptr addrspace(2)), align 1
  %val2 = call i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_elim3(i8 addrspace(1)* %arg1, i256 %arg2, i1 %cond) {
; CHECK-LABEL: @test_elim3(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @__sha3
; CHECK:       ret1:
; CHECK-NEXT:    call i256 @__sha3
; CHECK-NOT:     call i256 @__sha3
; CHECK:       ret2:
; CHECK-NOT:     call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(i8 addrspace(1)* %arg1, i256 %arg2, i1 false)
  br i1 %cond, label %check, label %ret2

check:
  call void @dummy()
  br label %ret1

ret1:
  %val2 = call i256 @__sha3(i8 addrspace(1)* %arg1, i256 %arg2, i1 false)
  %val3 = call i256 @__sha3(i8 addrspace(1)* %arg1, i256 %arg2, i1 false)
  %sum1 = add i256 %val2, %val3
  %res1 = add i256 %val1, %sum1
  ret i256 %res1

ret2:
  %val4 = call i256 @__sha3(i8 addrspace(1)* %arg1, i256 %arg2, i1 false)
  %res2 = add i256 %val1, %val4
  ret i256 %res2
}

define i256 @test_elim4(i8 addrspace(1)* %arg1, i256 %arg2, i1 %cond) {
; CHECK-LABEL: @test_elim4(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @__sha3
; CHECK:       ret1:
; CHECK-NOT:     call i256 @__sha3
; CHECK-NOT:     call i256 @__sha3
; CHECK:       ret2:
; CHECK-NOT:     call i256 @__sha3
;
entry:
  %val1 = call i256 @__sha3(i8 addrspace(1)* %arg1, i256 %arg2, i1 false)
  br i1 %cond, label %check, label %ret2

check:
  call void @dummy() readnone
  br label %ret1

ret1:
  %val2 = call i256 @__sha3(i8 addrspace(1)* %arg1, i256 %arg2, i1 false)
  %val3 = call i256 @__sha3(i8 addrspace(1)* %arg1, i256 %arg2, i1 false)
  %sum1 = add i256 %val2, %val3
  %res1 = add i256 %val1, %sum1
  ret i256 %res1

ret2:
  %val4 = call i256 @__sha3(i8 addrspace(1)* %arg1, i256 %arg2, i1 false)
  %res2 = add i256 %val1, %val4
  ret i256 %res2
}

; TODO: CPR-1510 Eliminate calls based on the input data.
define i256 @test_elim5(i256 %input_data) {
; CHECK-LABEL: @test_elim5(
; CHECK:         call i256 @__sha3
; CHECK:         call i256 @__sha3
;
entry:
  store i256 %input_data, ptr addrspace(1) null, align 1
  store i256 %input_data, ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1)), align 1
  %val1 = call i256 @__sha3(ptr addrspace(1) null, i256 32, i1 true)
  %val2 = call i256 @__sha3(ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1)), i256 32, i1 true)
  %res = add i256 %val1, %val2
  ret i256 %res
}

declare void @dummy()
declare i256 @__sha3(i8 addrspace(1)* %0, i256 %1, i1 %throw_at_failure) #0

attributes #0 = { argmemonly nofree null_pointer_is_valid readonly }
