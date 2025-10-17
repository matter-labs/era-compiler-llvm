; RUN: opt -passes=aa-eval -aa-pipeline=basic-aa -print-all-alias-modref-info -disable-output < %s 2>&1 | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

; CHECK-LABEL: Function: test_offset_i8_noalias
; CHECK: MayAlias: i8 addrspace(1)* %inttoptr1, i8 addrspace(1)* %inttoptr2
define void @test_offset_i8_noalias(ptr addrspace(1) %addr) {
  %ptrtoint = ptrtoint ptr addrspace(1) %addr to i8
  %add1 = add i8 %ptrtoint, 32
  %inttoptr1 = inttoptr i8 %add1 to ptr addrspace(1)
  store i8 3, ptr addrspace(1) %inttoptr1, align 1
  %add2 = add i8 %ptrtoint, 33
  %inttoptr2 = inttoptr i8 %add2 to ptr addrspace(1)
  store i8 58, ptr addrspace(1) %inttoptr2, align 1
  ret void
}

; CHECK-LABEL: Function: test_offset_i512_noalias
; CHECK: MayAlias: i512 addrspace(1)* %inttoptr1, i512 addrspace(1)* %inttoptr2
define void @test_offset_i512_noalias(ptr addrspace(1) %addr) {
  %ptrtoint = ptrtoint ptr addrspace(1) %addr to i512
  %add1 = add i512 %ptrtoint, 32
  %inttoptr1 = inttoptr i512 %add1 to ptr addrspace(1)
  store i512 3, ptr addrspace(1) %inttoptr1, align 1
  %add2 = add i512 %ptrtoint, 96
  %inttoptr2 = inttoptr i512 %add2 to ptr addrspace(1)
  store i512 58, ptr addrspace(1) %inttoptr2, align 1
  ret void
}

; CHECK-LABEL: Function: test_offset_mustalias
; CHECK: MayAlias: i256 addrspace(1)* %inttoptr1, i256 addrspace(1)* %inttoptr2
define void @test_offset_mustalias(ptr addrspace(1) %addr) {
  %ptrtoint = ptrtoint ptr addrspace(1) %addr to i256
  %inttoptr1 = inttoptr i256 %ptrtoint to ptr addrspace(1)
  store i256 3, ptr addrspace(1) %inttoptr1, align 1
  %inttoptr2 = inttoptr i256 %ptrtoint to ptr addrspace(1)
  store i256 58, ptr addrspace(1) %inttoptr2, align 1
  ret void
}

; CHECK-LABEL: Function: test_offset_noalias1
; CHECK: MayAlias: i256 addrspace(1)* %inttoptr1, i256 addrspace(1)* %inttoptr2
define void @test_offset_noalias1(ptr addrspace(1) %addr) {
  %ptrtoint = ptrtoint ptr addrspace(1) %addr to i256
  %add1 = add i256 %ptrtoint, 32
  %inttoptr1 = inttoptr i256 %add1 to ptr addrspace(1)
  store i256 3, ptr addrspace(1) %inttoptr1, align 1
  %add2 = add i256 %ptrtoint, 64
  %inttoptr2 = inttoptr i256 %add2 to ptr addrspace(1)
  store i256 58, ptr addrspace(1) %inttoptr2, align 1
  ret void
}

; CHECK-LABEL: Function: test_offset_noalias2
; CHECK: MayAlias: i256 addrspace(1)* %inttoptr1, i256 addrspace(1)* %inttoptr2
define void @test_offset_noalias2(ptr addrspace(1) %addr) {
  %ptrtoint = ptrtoint ptr addrspace(1) %addr to i256
  %add1 = add i256 %ptrtoint, -32
  %inttoptr1 = inttoptr i256 %add1 to ptr addrspace(1)
  store i256 3, ptr addrspace(1) %inttoptr1, align 1
  %add2 = add i256 %ptrtoint, -64
  %inttoptr2 = inttoptr i256 %add2 to ptr addrspace(1)
  store i256 58, ptr addrspace(1) %inttoptr2, align 1
  ret void
}

; CHECK-LABEL: Function: test_offset_noalias3
; CHECK: MayAlias: i256 addrspace(1)* %inttoptr1, i256 addrspace(1)* %inttoptr2
define void @test_offset_noalias3(ptr addrspace(1) %addr) {
  %ptrtoint = ptrtoint ptr addrspace(1) %addr to i256
  %add1 = sub i256 %ptrtoint, 32
  %inttoptr1 = inttoptr i256 %add1 to ptr addrspace(1)
  store i256 3, ptr addrspace(1) %inttoptr1, align 1
  %add2 = sub i256 %ptrtoint, 64
  %inttoptr2 = inttoptr i256 %add2 to ptr addrspace(1)
  store i256 58, ptr addrspace(1) %inttoptr2, align 1
  ret void
}

; CHECK-LABEL: Function: test_offset_partialalias1
; CHECK: MayAlias: i256 addrspace(1)* %inttoptr1, i256 addrspace(1)* %inttoptr2
define void @test_offset_partialalias1(ptr addrspace(1) %addr) {
  %ptrtoint = ptrtoint ptr addrspace(1) %addr to i256
  %add1 = add i256 %ptrtoint, 32
  %inttoptr1 = inttoptr i256 %add1 to ptr addrspace(1)
  store i256 3, ptr addrspace(1) %inttoptr1, align 1
  %add2 = add i256 %ptrtoint, 48
  %inttoptr2 = inttoptr i256 %add2 to ptr addrspace(1)
  store i256 58, ptr addrspace(1) %inttoptr2, align 1
  ret void
}

; CHECK-LABEL: Function: test_offset_partialalias2
; CHECK: MayAlias: i256 addrspace(1)* %inttoptr1, i256 addrspace(1)* %inttoptr2
define void @test_offset_partialalias2(ptr addrspace(1) %addr) {
  %ptrtoint = ptrtoint ptr addrspace(1) %addr to i256
  %add1 = add i256 %ptrtoint, -32
  %inttoptr1 = inttoptr i256 %add1 to ptr addrspace(1)
  store i256 3, ptr addrspace(1) %inttoptr1, align 1
  %add2 = add i256 %ptrtoint, -48
  %inttoptr2 = inttoptr i256 %add2 to ptr addrspace(1)
  store i256 58, ptr addrspace(1) %inttoptr2, align 1
  ret void
}

; CHECK-LABEL: Function: test_offset_partialalias3
; CHECK: MayAlias: i256 addrspace(1)* %inttoptr1, i256 addrspace(1)* %inttoptr2
define void @test_offset_partialalias3(ptr addrspace(1) %addr) {
  %ptrtoint = ptrtoint ptr addrspace(1) %addr to i256
  %add1 = sub i256 %ptrtoint, 32
  %inttoptr1 = inttoptr i256 %add1 to ptr addrspace(1)
  store i256 3, ptr addrspace(1) %inttoptr1, align 1
  %add2 = sub i256 %ptrtoint, 48
  %inttoptr2 = inttoptr i256 %add2 to ptr addrspace(1)
  store i256 58, ptr addrspace(1) %inttoptr2, align 1
  ret void
}

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
; CHECK: MayAlias: i256* %ptr0, i256 addrspace(1)* %ptr1
; CHECK: MayAlias: i256* %ptr0, i256 addrspace(2)* %ptr2
; CHECK: MayAlias: i256 addrspace(1)* %ptr1, i256 addrspace(2)* %ptr2
; CHECK: MayAlias: i256* %ptr0, i256 addrspace(4)* %ptr4
; CHECK: MayAlias: i256 addrspace(1)* %ptr1, i256 addrspace(4)* %ptr4
; CHECK: MayAlias: i256 addrspace(2)* %ptr2, i256 addrspace(4)* %ptr4
; CHECK: MayAlias: i256* %ptr0, i256 addrspace(5)* %ptr5
; CHECK: MayAlias: i256 addrspace(1)* %ptr1, i256 addrspace(5)* %ptr5
; CHECK: MayAlias: i256 addrspace(2)* %ptr2, i256 addrspace(5)* %ptr5
; CHECK: MayAlias: i256 addrspace(4)* %ptr4, i256 addrspace(5)* %ptr5
; CHECK: MayAlias: i256* %ptr0, i256 addrspace(6)* %ptr6
; CHECK: MayAlias: i256 addrspace(1)* %ptr1, i256 addrspace(6)* %ptr6
; CHECK: MayAlias: i256 addrspace(2)* %ptr2, i256 addrspace(6)* %ptr6
; CHECK: MayAlias: i256 addrspace(4)* %ptr4, i256 addrspace(6)* %ptr6
; CHECK: MayAlias: i256 addrspace(5)* %ptr5, i256 addrspace(6)* %ptr6
define void @test_noalias() {
  %ptr0 = inttoptr i256 32 to ptr
  %ptr1 = inttoptr i256 32 to ptr addrspace(1)
  %ptr2 = inttoptr i256 32 to ptr addrspace(2)
  %ptr4 = inttoptr i256 32 to ptr addrspace(4)
  %ptr5 = inttoptr i256 32 to ptr addrspace(5)
  %ptr6 = inttoptr i256 32 to ptr addrspace(6)
  store i256 1, ptr %ptr0, align 64
  store i256 1, ptr addrspace(1) %ptr1, align 64
  store i256 1, ptr addrspace(2) %ptr2, align 64
  store i256 1, ptr addrspace(4) %ptr4, align 64
  store i256 1, ptr addrspace(5) %ptr5, align 64
  store i256 1, ptr addrspace(6) %ptr6, align 64
  ret void
}

; CHECK-LABEL: Function: test_as1_noalias
; CHECK: MayAlias: i256 addrspace(1)* %ptr1, i256 addrspace(1)* %ptr2
define void @test_as1_noalias() {
  %ptr1 = inttoptr i256 32 to ptr addrspace(1)
  %ptr2 = inttoptr i256 64 to ptr addrspace(1)
  store i256 2, ptr addrspace(1) %ptr1, align 64
  store i256 1, ptr addrspace(1) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as1_mustalias
; CHECK: MayAlias: i256 addrspace(1)* %ptr1, i256 addrspace(1)* %ptr2
define void @test_as1_mustalias() {
  %ptr1 = inttoptr i256 32 to ptr addrspace(1)
  %ptr2 = inttoptr i256 32 to ptr addrspace(1)
  store i256 2, ptr addrspace(1) %ptr1, align 64
  store i256 1, ptr addrspace(1) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as1_partialalias
; CHECK: MayAlias: i256 addrspace(1)* %ptr1, i256 addrspace(1)* %ptr2
define void @test_as1_partialalias() {
  %ptr1 = inttoptr i256 32 to ptr addrspace(1)
  %ptr2 = inttoptr i256 48 to ptr addrspace(1)
  store i256 2, ptr addrspace(1) %ptr1, align 64
  store i256 1, ptr addrspace(1) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as5_noalias
; CHECK: MayAlias: i256 addrspace(5)* %ptr1, i256 addrspace(5)* %ptr2
define void @test_as5_noalias() {
  %ptr1 = inttoptr i256 0 to ptr addrspace(5)
  %ptr2 = inttoptr i256 1 to ptr addrspace(5)
  store i256 2, ptr addrspace(5) %ptr1, align 64
  store i256 1, ptr addrspace(5) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as5_mustalias
; CHECK: MayAlias: i256 addrspace(5)* %ptr1, i256 addrspace(5)* %ptr2
define void @test_as5_mustalias() {
  %ptr1 = inttoptr i256 0 to ptr addrspace(5)
  %ptr2 = inttoptr i256 0 to ptr addrspace(5)
  store i256 2, ptr addrspace(5) %ptr1, align 64
  store i256 1, ptr addrspace(5) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as6_noalias
; CHECK: MayAlias: i256 addrspace(6)* %ptr1, i256 addrspace(6)* %ptr2
define void @test_as6_noalias() {
  %ptr1 = inttoptr i256 0 to ptr addrspace(6)
  %ptr2 = inttoptr i256 1 to ptr addrspace(6)
  store i256 2, ptr addrspace(6) %ptr1, align 64
  store i256 1, ptr addrspace(6) %ptr2, align 64
  ret void
}

; CHECK-LABEL: Function: test_as6_mustalias
; CHECK: MayAlias: i256 addrspace(6)* %ptr1, i256 addrspace(6)* %ptr2
define void @test_as6_mustalias() {
  %ptr1 = inttoptr i256 0 to ptr addrspace(6)
  %ptr2 = inttoptr i256 0 to ptr addrspace(6)
  store i256 2, ptr addrspace(6) %ptr1, align 64
  store i256 1, ptr addrspace(6) %ptr2, align 64
  ret void
}
