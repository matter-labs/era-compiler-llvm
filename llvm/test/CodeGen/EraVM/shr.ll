; RUN: llc -opaque-pointers < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: shrrrr
define i256 @shrrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: shr r1, r2, r1
  %res = lshr i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: shrirr
define i256 @shrirr(i256 %rs1) nounwind {
; CHECK: shr 42, r1, r1
  %res = lshr i256 42, %rs1
  ret i256 %res
}

; CHECK-LABEL: shrxrr
define i256 @shrxrr(i256 %rs1) nounwind {
; CHECK: shr.s 42, r1, r1
  %res = lshr i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: shrcrr
define i256 @shrcrr(i256 %rs1) nounwind {
; CHECK: shr @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = lshr i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: shryrr
define i256 @shryrr(i256 %rs1) nounwind {
; CHECK: shr.s @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = lshr i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: shrsrr
define i256 @shrsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shr stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = lshr i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: shrzrr
define i256 @shrzrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shr.s stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = lshr i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: shrrrs
define void @shrrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = lshr i256 %rs1, %rs2
; CHECK: shr r1, r2, stack-[1]
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shrirs
define void @shrirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shr 42, r1, stack-[1]
  %res = lshr i256 42, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shrxrs
define void @shrxrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shr.s 42, r1, stack-[1]
  %res = lshr i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shrcrs
define void @shrcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shr @val[0], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = lshr i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shryrs
define void @shryrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: shr.s @val[0], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = lshr i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: shrsrs
define void @shrsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: shr stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = lshr i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: shrzrs
define void @shrzrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: shr.s stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = lshr i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret void
}
