; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32"
target triple = "syncvm"

; CHECK-LABEL: ptr_add.i8
define i8 addrspace(3)* @ptr_add.i8(i8 addrspace(3)* %ptr) #0 {
  ; CHECK: ptr.add.s 5, r1, r1
  %ptr2 = getelementptr inbounds i8, i8 addrspace (3)* %ptr, i256 5
  ret i8 addrspace(3)* %ptr2
}

; CHECK-LABEL: ptr_add.i64
define i64 addrspace(3)* @ptr_add.i64(i64 addrspace(3)* %ptr) #0 {
  ; CHECK: ptr.add.s 40, r1, r1
  %ptr2 = getelementptr inbounds i64, i64 addrspace (3)* %ptr, i256 5
  ret i64 addrspace(3)* %ptr2
}

; CHECK-LABEL: ptr_add.i256
define i256 addrspace(3)* @ptr_add.i256(i256 addrspace(3)* %ptr) #0 {
  ; CHECK: ptr.add.s 160, r1, r1
  %ptr2 = getelementptr inbounds i256, i256 addrspace (3)* %ptr, i256 5
  ret i256 addrspace(3)* %ptr2
}

; CHECK-LABEL: ptr_add.i8.nonconst
define i8 addrspace(3)* @ptr_add.i8.nonconst(i8 addrspace(3)* %ptr, i256 %idx) #0 {
  ; CHECK: ptr.add r1, r2, r1
  %ptr2 = getelementptr inbounds i8, i8 addrspace (3)* %ptr, i256 %idx
  ret i8 addrspace(3)* %ptr2
}

; CHECK-LABEL: ptr_add.i64.nonconst
define i64 addrspace(3)* @ptr_add.i64.nonconst(i64 addrspace(3)* %ptr, i256 %idx) #0 {
  ; shl.s	3, r2, r2
  ; CHECK: ptr.add r1, r{{[0-9]+}}, r1
  %ptr2 = getelementptr inbounds i64, i64 addrspace (3)* %ptr, i256 %idx
  ret i64 addrspace(3)* %ptr2
}

; CHECK-LABEL: ptr_add.i256.nonconst
define i256 addrspace(3)* @ptr_add.i256.nonconst(i256 addrspace(3)* %ptr, i256 %idx) #0 {
  ; shl.s	5, r2, r2
  ; CHECK: ptr.add r1, r{{[0-9]+}}, r1
  %ptr2 = getelementptr inbounds i256, i256 addrspace (3)* %ptr, i256 %idx
  ret i256 addrspace(3)* %ptr2
}

; Test for pointer packing emitting
define i8 addrspace(3)* @ptr_pack(i8 addrspace(3)* %ptr) #0 {
  ; TODO: Should be ptr.pack.s 1024, r1
  ; CHECK: add 1024, r0, r2
  ; CHECK: ptr.pack r1, r2
  %ptr2 = call i8 addrspace(3) *(i8 addrspace(3)*, i256) @llvm.syncvm.ptr.pack(i8 addrspace(3) * %ptr, i256 1024);
  ret i8 addrspace(3)* %ptr2;
}

declare i8 addrspace(3) * @llvm.syncvm.ptr.pack(i8 addrspace(3) *, i256)

