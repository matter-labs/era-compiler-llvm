; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: addrrr
define i256 @addrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: add r1, r2, r1
  %res = add i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: addirr
define i256 @addirr(i256 %rs1) nounwind {
; CHECK: add 42, r1, r1
  %res = add i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: addcrr
define i256 @addcrr(i256 %rs1) nounwind {
; CHECK: add @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = add i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: addsrr
define i256 @addsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: add stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res = add i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: addrrs
define void @addrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = add i256 %rs1, %rs2
; CHECK: add r1, r2, stack-[1]
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: addirs
define void @addirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: add 42, r1, stack-[1]
  %res = add i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: addcrs
define void @addcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: add @val[0], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = add i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void 
}

; CHECK-LABEL: addsrs
define void @addsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: add stack-[2], r1, stack-[1]
  %val = load i256, i256* %valptr
  %res = add i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: addneg
define i256 @addneg(i256 %rs1) nounwind {
  %res = add i256 %rs1, -65535
; CHECK; sub.s   65535, r1, r1
  ret i256 %res
}

