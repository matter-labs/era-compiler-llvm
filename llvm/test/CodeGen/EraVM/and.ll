; RUN: llc -opaque-pointers < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: andrrr
define i256 @andrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: and r1, r2, r1
  %res = and i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: andirr
define i256 @andirr(i256 %rs1) nounwind {
; CHECK: and 42, r1, r1
  %res = and i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: andcrr
define i256 @andcrr(i256 %rs1) nounwind {
; CHECK: and @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = and i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: andsrr
define i256 @andsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: and stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = and i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: andrrs
define void @andrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = and i256 %rs1, %rs2
; CHECK: and r1, r2, stack-[1]
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: andirs
define void @andirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: and 42, r1, stack-[1]
  %res = and i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: andcrs
define void @andcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: and @val[0], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = and i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void 
}

; CHECK-LABEL: andsrs
define void @andsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: and stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = and i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}
