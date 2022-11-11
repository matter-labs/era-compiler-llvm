; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: orrrr
define i256 @orrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: or r1, r2, r1
  %res = or i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: orirr
define i256 @orirr(i256 %rs1) nounwind {
; CHECK: or 42, r1, r1
  %res = or i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: orcrr
define i256 @orcrr(i256 %rs1) nounwind {
; CHECK: or @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = or i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: orsrr
define i256 @orsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: or stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = or i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: orrrs
define void @orrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = or i256 %rs1, %rs2
; CHECK: or r1, r2, stack-[1]
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: orirs
define void @orirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: or 42, r1, stack-[1]
  %res = or i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: orcrs
define void @orcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: or @val[0], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = or i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void 
}

; CHECK-LABEL: orsrs
define void @orsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: or stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = or i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}
