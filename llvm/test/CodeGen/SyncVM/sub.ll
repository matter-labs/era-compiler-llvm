; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: subrrr
define i256 @subrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: sub r1, r2, r1
  %res = sub i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: subirr
define i256 @subirr(i256 %rs1) nounwind {
; CHECK: sub 42, r1, r1
  %res = sub i256 42, %rs1
  ret i256 %res
}

; CHECK-LABEL: subxrr
define i256 @subxrr(i256 %rs1) nounwind {
; CHECK: sub.s 42, r1, r1
  %res = sub i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: sub2add
define i256 @sub2add(i256 %rs1) nounwind {
; CHECK: add 42, r1, r1
  %res = sub i256 %rs1, -42
  ret i256 %res
}

; CHECK-LABEL: subcrr
define i256 @subcrr(i256 %rs1) nounwind {
; CHECK: sub @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = sub i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: subyrr
define i256 @subyrr(i256 %rs1) nounwind {
; CHECK: sub.s @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = sub i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: subsrr
define i256 @subsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: sub stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = sub i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: subzrr
define i256 @subzrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: sub.s stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = sub i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: subrrs
define void @subrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = sub i256 %rs1, %rs2
; CHECK: sub r1, r2, stack-[1]
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: subirs
define void @subirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: sub 42, r1, stack-[1]
  %res = sub i256 42, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: subxrs
define void @subxrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: sub.s 42, r1, stack-[1]
  %res = sub i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: subcrs
define void @subcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: sub @val[0], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = sub i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret void 
}

; CHECK-LABEL: subyrs
define void @subyrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: sub.s @val[0], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = sub i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void 
}

; CHECK-LABEL: subsrs
define void @subsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: sub stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = sub i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: subzrs
define void @subzrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: sub.s stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = sub i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: sub_small_int
define i256 @sub_small_int(i256 %rs1) nounwind {
  %res = sub i256 %rs1, 65535
; CHECK: sub.s   65535, r1, r1
  ret i256 %res
}

