; RUN: llc --disable-eravm-scalar-opt-passes -enable-eravm-combine-addressing-mode=false < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: shlrrr
define i256 @shlrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: shl r1, r2, r1
  %res = shl i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: shlirr
define i256 @shlirr(i256 %rs1) nounwind {
; CHECK: shl 42, r1, r1
  %res = shl i256 42, %rs1
  ret i256 %res
}

; CHECK-LABEL: shlxrr
define i256 @shlxrr(i256 %rs1) nounwind {
; CHECK: shl.s 42, r1, r1
  %res = shl i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: shlcrr
define i256 @shlcrr(i256 %rs1) nounwind {
; CHECK: shl @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = shl i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: shlcrr_cp
define i256 @shlcrr_cp(i256 %rs1) nounwind {
; CHECK: shl @CPI4_0[0], r1, r1
  %res = shl i256 123456789, %rs1
  ret i256 %res
}

; CHECK-LABEL: shlyrr
define i256 @shlyrr(i256 %rs1) nounwind {
; CHECK: shl.s @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = shl i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: shlsrr
define i256 @shlsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shl stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = shl i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: shlzrr
define i256 @shlzrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shl.s stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = shl i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: shlrrs
define void @shlrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = shl i256 %rs1, %rs2
; CHECK: shl r1, r2, stack-[1]
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shlirs
define void @shlirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shl 42, r1, stack-[1]
  %res = shl i256 42, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shlxrs
define void @shlxrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shl.s 42, r1, stack-[1]
  %res = shl i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shlcrs
define void @shlcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shl @val[0], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = shl i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shlcrs_cp
define void @shlcrs_cp(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shl @CPI12_0[0], r1, stack-[1]
  %res = shl i256 123456789, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shlyrs
define void @shlyrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shl.s @val[0], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = shl i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shlsrs
define void @shlsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: shl stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = shl i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: shlzrs
define void @shlzrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: shl.s stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = shl i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret void
}
