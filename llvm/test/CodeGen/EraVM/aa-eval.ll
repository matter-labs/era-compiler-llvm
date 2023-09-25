; RUN: opt -passes=aa-eval -aa-pipeline=basic-aa,eravm-aa -print-all-alias-modref-info -disable-output < %s 2>&1 | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; TODO: CPR-1337 Enable all pointer sizes.
; CHECK-LABEL: Function: test_as1_i8_noalias
; CHECK: MayAlias: i8 addrspace(1)* %ptr1, i8 addrspace(1)* %ptr2
define void @test_as1_i8_noalias() {
  %ptr1 = inttoptr i8 32 to ptr addrspace(1)
  %ptr2 = inttoptr i8 64 to ptr addrspace(1)
  store i8 2, ptr addrspace(1) %ptr1, align 64
  store i8 1, ptr addrspace(1) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_unknown_as
; CHECK: MayAlias: i256 addrspace(1)* %ptr1, i256 addrspace(10)* %ptr2
define void @test_unknown_as() {
  %ptr1 = inttoptr i256 32 to ptr addrspace(1)
  %ptr2 = inttoptr i256 64 to ptr addrspace(10)
  store i256 2, ptr addrspace(1) %ptr1, align 64
  store i256 1, ptr addrspace(10) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_mayalias
; CHECK: MayAlias: i256* %0, i256* %1
define void @test_mayalias(ptr %0, ptr %1) {
  store i256 2, ptr %0, align 64
  store i256 1, ptr %1, align 64
  ret void
}

; CHECK-LABEL: Function: test_noalias_fallback
; CHECK: NoAlias: i256* %0, i256* %1
define void @test_noalias_fallback(ptr noalias %0, ptr noalias %1) {
  store i256 2, ptr %0, align 64
  store i256 1, ptr %1, align 64
  ret void
}

; CHECK-LABEL: Function: test_noalias
; CHECK: NoAlias: i256* %ptr0, i256 addrspace(1)* %ptr1
; CHECK: NoAlias: i256* %ptr0, i256 addrspace(2)* %ptr2
; CHECK: NoAlias: i256 addrspace(1)* %ptr1, i256 addrspace(2)* %ptr2
; CHECK: NoAlias: i256* %ptr0, i256 addrspace(4)* %ptr4
; CHECK: NoAlias: i256 addrspace(1)* %ptr1, i256 addrspace(4)* %ptr4
; CHECK: NoAlias: i256 addrspace(2)* %ptr2, i256 addrspace(4)* %ptr4
; CHECK: NoAlias: i256* %ptr0, i256 addrspace(5)* %ptr5
; CHECK: NoAlias: i256 addrspace(1)* %ptr1, i256 addrspace(5)* %ptr5
; CHECK: NoAlias: i256 addrspace(2)* %ptr2, i256 addrspace(5)* %ptr5
; CHECK: NoAlias: i256 addrspace(4)* %ptr4, i256 addrspace(5)* %ptr5
define void @test_noalias() {
  %ptr0 = inttoptr i256 32 to ptr
  %ptr1 = inttoptr i256 32 to ptr addrspace(1)
  %ptr2 = inttoptr i256 32 to ptr addrspace(2)
  %ptr4 = inttoptr i256 32 to ptr addrspace(4)
  %ptr5 = inttoptr i256 32 to ptr addrspace(5)
  store i256 1, ptr %ptr0, align 64
  store i256 1, ptr addrspace(1) %ptr1, align 64
  store i256 1, ptr addrspace(2) %ptr2, align 64
  store i256 1, ptr addrspace(4) %ptr4, align 64
  store i256 1, ptr addrspace(5) %ptr5, align 64
  ret void
}

; CHECK-LABEL: Function: test_as1_noalias
; CHECK: NoAlias: i256 addrspace(1)* %ptr1, i256 addrspace(1)* %ptr2
define void @test_as1_noalias() {
  %ptr1 = inttoptr i256 32 to ptr addrspace(1)
  %ptr2 = inttoptr i256 64 to ptr addrspace(1)
  store i256 2, ptr addrspace(1) %ptr1, align 64
  store i256 1, ptr addrspace(1) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as1_mustalias
; CHECK: MustAlias: i256 addrspace(1)* %ptr1, i256 addrspace(1)* %ptr2
define void @test_as1_mustalias() {
  %ptr1 = inttoptr i256 32 to ptr addrspace(1)
  %ptr2 = inttoptr i256 32 to ptr addrspace(1)
  store i256 2, ptr addrspace(1) %ptr1, align 64
  store i256 1, ptr addrspace(1) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as1_partialalias
; CHECK: PartialAlias: i256 addrspace(1)* %ptr1, i256 addrspace(1)* %ptr2
define void @test_as1_partialalias() {
  %ptr1 = inttoptr i256 32 to ptr addrspace(1)
  %ptr2 = inttoptr i256 48 to ptr addrspace(1)
  store i256 2, ptr addrspace(1) %ptr1, align 64
  store i256 1, ptr addrspace(1) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as2_noalias
; CHECK: NoAlias: i256 addrspace(2)* %ptr1, i256 addrspace(2)* %ptr2
define void @test_as2_noalias() {
  %ptr1 = inttoptr i256 32 to ptr addrspace(2)
  %ptr2 = inttoptr i256 64 to ptr addrspace(2)
  store i256 2, ptr addrspace(2) %ptr1, align 64
  store i256 1, ptr addrspace(2) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as2_mustalias
; CHECK: MustAlias: i256 addrspace(2)* %ptr1, i256 addrspace(2)* %ptr2
define void @test_as2_mustalias() {
  %ptr1 = inttoptr i256 32 to ptr addrspace(2)
  %ptr2 = inttoptr i256 32 to ptr addrspace(2)
  store i256 2, ptr addrspace(2) %ptr1, align 64
  store i256 1, ptr addrspace(2) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as2_partialalias
; CHECK: PartialAlias: i256 addrspace(2)* %ptr1, i256 addrspace(2)* %ptr2
define void @test_as2_partialalias() {
  %ptr1 = inttoptr i256 32 to ptr addrspace(2)
  %ptr2 = inttoptr i256 48 to ptr addrspace(2)
  store i256 2, ptr addrspace(2) %ptr1, align 64
  store i256 1, ptr addrspace(2) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as5_noalias
; CHECK: NoAlias: i256 addrspace(5)* %ptr1, i256 addrspace(5)* %ptr2
define void @test_as5_noalias() {
  %ptr1 = inttoptr i256 0 to ptr addrspace(5)
  %ptr2 = inttoptr i256 1 to ptr addrspace(5)
  store i256 2, ptr addrspace(5) %ptr1, align 64
  store i256 1, ptr addrspace(5) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as5_mustalias
; CHECK: MustAlias: i256 addrspace(5)* %ptr1, i256 addrspace(5)* %ptr2
define void @test_as5_mustalias() {
  %ptr1 = inttoptr i256 0 to ptr addrspace(5)
  %ptr2 = inttoptr i256 0 to ptr addrspace(5)
  store i256 2, ptr addrspace(5) %ptr1, align 64
  store i256 1, ptr addrspace(5) %ptr2, align 64
  ret void
}
