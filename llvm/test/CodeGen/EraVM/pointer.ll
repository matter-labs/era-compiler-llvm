; RUN: llc -opaque-pointers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32"
target triple = "eravm"

; Test for pointer arithmetic emitting

; check known offset

; CHECK-LABEL: ptr_add_i8
define i8 @ptr_add_i8(i8 addrspace(3) * %ptr) #0 {
  ; CHECK: ptr.add.s 5, r1, r1
  %ptr2 = getelementptr inbounds i8, i8 addrspace (3) * %ptr, i256 5
  %v = load i8, i8 addrspace(3)* %ptr2
  ret i8 %v
}

; CHECK-LABEL: ptr_add_i16
define i16 @ptr_add_i16(i16 addrspace(3) * %ptr) #0 {
  ; CHECK: ptr.add.s 10, r1, r1
  %ptr2 = getelementptr inbounds i16, i16 addrspace (3) * %ptr, i256 5
  %v = load i16, i16 addrspace(3)* %ptr2
  ret i16 %v
}

; CHECK-LABEL: ptr_add_i32
define i32 @ptr_add_i32(i32 addrspace(3) *  %ptr) #0 {
  ; CHECK: ptr.add.s 20, r1, r1
  %ptr2 = getelementptr inbounds i32, i32 addrspace (3) * %ptr, i256 5
  %v = load i32, i32 addrspace(3)* %ptr2
  ret i32 %v
}

; CHECK-LABEL: ptr_add_i64
define i64 @ptr_add_i64(i64 addrspace(3) * %ptr) #0 {
  ; CHECK: ptr.add.s 40, r1, r1
  %ptr2 = getelementptr inbounds i64, i64 addrspace (3) * %ptr, i256 5
  %v = load i64, i64 addrspace(3)* %ptr2
  ret i64 %v
}

; CHECK-LABEL: ptr_add_i256
define i256 @ptr_add_i256(i256 addrspace(3) *  %ptr) #0 {
  ; CHECK: ptr.add.s 160, r1, r1
  %ptr2 = getelementptr inbounds i256, i256 addrspace (3) * %ptr, i256 5
  %v = load i256, i256 addrspace(3)* %ptr2
  ret i256 %v
}

; check variable offsets

; CHECK-LABEL: ptr_add_i8_2
define i8 @ptr_add_i8_2(i8 addrspace(3) *  %ptr, i256 %idx) #0 {
  ; CHECK-NOT: shl
  ; CHECK: ptr.add r1, r{{[0-9]+}}, r1
  %ptr2 = getelementptr inbounds i8, i8 addrspace (3) * %ptr, i256 %idx
  %v = load i8, i8 addrspace(3)* %ptr2
  ret i8 %v
}

; CHECK-LABEL: ptr_add_i16_2
define i16 @ptr_add_i16_2(i16 addrspace(3) * %ptr, i256 %idx) #0 {
  ; CHECK: shl.s 1, r2, r2
  ; CHECK: ptr.add r1, r{{[0-9]+}}, r1
  %ptr2 = getelementptr inbounds i16, i16 addrspace (3) * %ptr, i256 %idx
  %v = load i16, i16 addrspace(3)* %ptr2
  ret i16 %v
}

; CHECK-LABEL: ptr_add_i32_2
define i32 @ptr_add_i32_2(i32 addrspace(3) * %ptr, i256 %idx) #0 {
  ; CHECK: shl.s 2, r2, r2
  ; CHECK: ptr.add r1, r{{[0-9]+}}, r1
  %ptr2 = getelementptr inbounds i32, i32 addrspace (3) * %ptr, i256 %idx
  %v = load i32, i32 addrspace(3)* %ptr2
  ret i32 %v
}

; CHECK-LABEL: ptr_add_i64_2
define i64 @ptr_add_i64_2(i64 addrspace(3) * %ptr, i256 %idx) #0 {
  ; CHECK: shl.s 3, r2, r2
  ; CHECK: ptr.add r1, r{{[0-9]+}}, r1
  %ptr2 = getelementptr inbounds i64, i64 addrspace (3) * %ptr, i256 %idx
  %v = load i64, i64 addrspace(3)* %ptr2
  ret i64 %v
}

; CHECK-LABEL: ptr_add_i256_2
define i256 @ptr_add_i256_2(i256 addrspace(3) * %ptr, i256 %idx) #0 {
  ; CHECK: shl.s 5, r2, r2
  ; CHECK: ptr.add r1, r{{[0-9]+}}, r1
  %ptr2 = getelementptr inbounds i256, i256 addrspace (3) * %ptr, i256 %idx
  %v = load i256, i256 addrspace(3)* %ptr2
  ret i256 %v
}

; CHECK-LABEL: ptr_pack
define i8 addrspace(3) * @ptr_pack(i8 addrspace(3) * %ptr) #0 {
  ; CHECK: ptr.pack.s 1024, r1, r1
  %ptr2 = call i8 addrspace(3) *(i8 addrspace(3) *, i256) @llvm.eravm.ptr.pack(i8 addrspace(3) * %ptr, i256 1024);
  ret i8 addrspace(3) * %ptr2;
}

; CHECK-LABEL: ptr_pack_variable_index
define i8 addrspace(3) * @ptr_pack_variable_index(i8 addrspace(3) * %ptr, i256 %idx) #0 {
  ; CHECK: ptr.pack r1, r2
  %ptr2 = call i8 addrspace(3) *(i8 addrspace(3) *, i256) @llvm.eravm.ptr.pack(i8 addrspace(3) * %ptr, i256 %idx);
  ret i8 addrspace(3) * %ptr2;
}

declare i8 addrspace(3) * @llvm.eravm.ptr.pack(i8 addrspace(3) *, i256)

