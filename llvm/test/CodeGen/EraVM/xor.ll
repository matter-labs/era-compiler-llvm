; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: xorrrr
define i256 @xorrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: xor r1, r2, r1
  %res = xor i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: xorirr
define i256 @xorirr(i256 %rs1) nounwind {
; CHECK: xor 42, r1, r1
  %res = xor i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: xorcrr
define i256 @xorcrr(i256 %rs1) nounwind {
; CHECK: xor @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = xor i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: xorsrr
define i256 @xorsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: xor stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = xor i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: xorrrs
define void @xorrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = xor i256 %rs1, %rs2
; CHECK: xor r1, r2, stack-[1]
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: xorirs
define void @xorirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: xor 42, r1, stack-[1]
  %res = xor i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: xorcrs
define void @xorcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: xor @val[0], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = xor i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void 
}

; CHECK-LABEL: xorsrs
define void @xorsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: xor stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = xor i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}
