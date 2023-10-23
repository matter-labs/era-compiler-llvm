; RUN: llc -opaque-pointers < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: movrrr
define i256 @movrrr(i256 %rs2, i256 %rs1) nounwind {
; CHECK: add r2, r0, r1
  ret i256 %rs1
}

; CHECK-LABEL: movirr
define i256 @movirr() nounwind {
; CHECK: add 42, r0, r1
  ret i256 42
}

; CHECK-LABEL: movcrr
define i256 @movcrr() nounwind {
; CHECK: add @val[0], r0, r1
  %val = load i256, i256 addrspace(4)* @val
  ret i256 %val
}

; CHECK-LABEL: movsrr
define i256 @movsrr() nounwind {
  %valptr = alloca i256
; CHECK: add stack-[1], r0, r1
  %val = load i256, i256* %valptr
  ret i256 %val
}

; CHECK-LABEL: movrrs
define void @movrrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: add r1, r0, stack-[1]
  store i256 %rs1, i256* %valptr
  ret void
}

; CHECK-LABEL: movirs
define void @movirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: add 42, r0, stack-[1]
  store i256 42, i256* %valptr
  ret void
}

; CHECK-LABEL: movcrs
define void @movcrs() nounwind {
  %valptr = alloca i256
; CHECK: add @val[0], r0, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  store i256 %val, i256* %valptr
  ret void
}

; CHECK-LABEL: movsrs
define void @movsrs() nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: stack-[2], r0, stack-[1]
  %val = load i256, i256* %valptr
  store i256 %val, i256* %destptr
  ret void
}
