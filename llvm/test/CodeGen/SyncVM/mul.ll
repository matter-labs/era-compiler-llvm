; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: mulrrr
define i256 @mulrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: mul r1, r2, r1
  %res = mul i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: mulirr
define i256 @mulirr(i256 %rs1) nounwind {
; CHECK: mul 42, r1, r1
  %res = mul i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: mulcrr
define i256 @mulcrr(i256 %rs1) nounwind {
; CHECK: mul code[val], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = mul i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: mulsrr
define i256 @mulsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: mul stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = mul i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: mulrrs
define void @mulrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = mul i256 %rs1, %rs2
; CHECK: mul r1, r2, stack-[1]
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: mulirs
define void @mulirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: mul 42, r1, stack-[1]
  %res = mul i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: mulcrs
define void @mulcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: mul code[val], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = mul i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void 
}

; CHECK-LABEL: mulsrs
define void @mulsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: mul stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = mul i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}
