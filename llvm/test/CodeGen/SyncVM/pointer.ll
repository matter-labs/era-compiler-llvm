; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; Test for pointer arithmetic emitting
define i8 @ptr_add(i8 addrspace(3) * addrspace(3)* %ptrptr) #0 {
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)* addrspace(3)* %ptrptr
  ; CHECK: ptr.add.s 5, r1, r1
  %ptr2 = getelementptr inbounds i8, i8 addrspace (3) * %ptr, i256 5
  %v = load i8, i8 addrspace(3)* %ptr2
  ret i8 %v
}

; Test for pointer arithmetic emitting
define i8 @ptr_add2(i8 addrspace(3) * addrspace(3)* %ptrptr, i256 %idx) #0 {
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)* addrspace(3)* %ptrptr
  ; CHECK: ptr.add r1, r{{[0-9]+}}, r1
  %ptr2 = getelementptr inbounds i8, i8 addrspace (3) * %ptr, i256 %idx
  %v = load i8, i8 addrspace(3)* %ptr2
  ret i8 %v
}

; Test for pointer packing emitting
define i8 addrspace(3) * @ptr_pack(i8 addrspace(3) * %ptr) #0 {
  ; CHECK: ptr.pack r1, r2
  %ptr2 = call i8 addrspace(3) *(i8 addrspace(3) *, i256) @llvm.syncvm.ptr.pack(i8 addrspace(3) * %ptr, i256 1024);
  ret i8 addrspace(3) * %ptr2;
}

declare i8 addrspace(3) * @llvm.syncvm.ptr.pack(i8 addrspace(3) *, i256)

