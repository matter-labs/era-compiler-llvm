; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: uremrrr
define i256 @uremrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: div r1, r2, r{{[0-9]+}}, r1
  %res = urem i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: uremirr
define i256 @uremirr(i256 %rs1) nounwind {
; CHECK: div 42, r1, r{{[0-9]+}}, r1
  %res = urem i256 42, %rs1
  ret i256 %res
}

; CHECK-LABEL: uremxrr
define i256 @uremxrr(i256 %rs1) nounwind {
; CHECK: div.s 42, r1, r{{[0-9]+}}, r1
  %res = urem i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: uremcrr
define i256 @uremcrr(i256 %rs1) nounwind {
; CHECK: div @val[0], r1, r{{[0-9]+}}, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = urem i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: uremyrr
define i256 @uremyrr(i256 %rs1) nounwind {
; CHECK: div.s @val[0], r1, r{{[0-9]+}}, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = urem i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: uremsrr
define i256 @uremsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div stack-[1], r1, r{{[0-9]+}}, r1
  %val = load i256, i256* %valptr
  %res = urem i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: uremzrr
define i256 @uremzrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s stack-[1], r1, r{{[0-9]+}}, r1
  %val = load i256, i256* %valptr
  %res = urem i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: uremrrs
define void @uremrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = urem i256 %rs1, %rs2
; CHECK: div r1, r2, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: uremirs
define void @uremirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div 42, r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %res = urem i256 42, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: uremxrs
define void @uremxrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s 42, r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %res = urem i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: uremcrs
define void @uremcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div @val[0], r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = urem i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: uremyrs
define void @uremyrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s @val[0], r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = urem i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: uremsrs
define void @uremsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: div stack-[2], r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %val = load i256, i256* %valptr
  %res = urem i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: uremzrs
define void @uremzrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: div.s stack-[2], r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %val = load i256, i256* %valptr
  %res = urem i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: udivrrr
define i256 @udivrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: div r1, r2, r1, r{{[0-9]+}}
  %res = udiv i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: udivirr
define i256 @udivirr(i256 %rs1) nounwind {
; CHECK: div 42, r1, r1, r{{[0-9]+}}
  %res = udiv i256 42, %rs1
  ret i256 %res
}

; CHECK-LABEL: udivxrr
define i256 @udivxrr(i256 %rs1) nounwind {
; CHECK: div.s 42, r1, r1, r{{[0-9]+}}
  %res = udiv i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: udivcrr
define i256 @udivcrr(i256 %rs1) nounwind {
; CHECK: div @val[0], r1, r1, r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: udivyrr
define i256 @udivyrr(i256 %rs1) nounwind {
; CHECK: div.s @val[0], r1, r1, r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: udivsrr
define i256 @udivsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div stack-[1], r1, r1, r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = udiv i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: udivzrr
define i256 @udivzrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s stack-[1], r1, r1, r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = udiv i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: udivrrs
define void @udivrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = udiv i256 %rs1, %rs2
; CHECK: div r1, r2, stack-[1], r{{[0-9]+}}
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: udivirs
define void @udivirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div 42, r1, stack-[1], r{{[0-9]+}}
  %res = udiv i256 42, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: udivxrs
define void @udivxrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s 42, r1, stack-[1], r{{[0-9]+}}
  %res = udiv i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: udivcrs
define void @udivcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div @val[0], r1, stack-[1], r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: udivyrs
define void @udivyrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s @val[0], r1, stack-[1], r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: udivsrs
define void @udivsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: div stack-[2], r1, stack-[1], r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = udiv i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: udivzrs
define void @udivzrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: div.s stack-[2], r1, stack-[1], r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = udiv i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: udivremrrrr
define i256 @udivremrrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: div r1, r2, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
; CHECK: add r[[REG2]], r[[REG1]], r1
  %res1 = urem i256 %rs1, %rs2
  %res2 = udiv i256 %rs1, %rs2
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: udivremirrr
define i256 @udivremirrr(i256 %rs1) nounwind {
; CHECK: div 42, r1, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
; CHECK: add r[[REG2]], r[[REG1]], r1
  %res1 = urem i256 42, %rs1
  %res2 = udiv i256 42, %rs1
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: udivremxrrr
define i256 @udivremxrrr(i256 %rs1) nounwind {
; CHECK: div.s 42, r1, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
; CHECK: add r[[REG2]], r[[REG1]], r1
  %res1 = urem i256 %rs1, 42
  %res2 = udiv i256 %rs1, 42
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: udivremcrrr
define i256 @udivremcrrr(i256 %rs1) nounwind {
; CHECK: div @val[0], r1, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
; CHECK: add r[[REG2]], r[[REG1]], r1
  %val = load i256, i256 addrspace(4)* @val
  %res1 = urem i256 %val, %rs1
  %res2 = udiv i256 %val, %rs1
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: udivremyrrr
define i256 @udivremyrrr(i256 %rs1) nounwind {
; CHECK: div.s @val[0], r1, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
; CHECK: add r[[REG2]], r[[REG1]], r1
  %val = load i256, i256 addrspace(4)* @val
  %res1 = urem i256 %rs1, %val
  %res2 = udiv i256 %rs1, %val
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: udivremsrrr
define i256 @udivremsrrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div stack-[1], r1, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
; CHECK: add r[[REG2]], r[[REG1]], r1
  %val = load i256, i256* %valptr
  %res1 = urem i256 %val, %rs1
  %res2 = udiv i256 %val, %rs1
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: udivremzrrr
define i256 @udivremzrrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s stack-[1], r1, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
; CHECK: add r[[REG2]], r[[REG1]], r1
  %val = load i256, i256* %valptr
  %res1 = urem i256 %rs1, %val
  %res2 = udiv i256 %rs1, %val
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: udivremrrsr
define i256 @udivremrrsr(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = udiv i256 %rs1, %rs2
  %rem = urem i256 %rs1, %rs2
; CHECK: div r1, r2, stack-[1], r1
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: udivremirsr
define i256 @udivremirsr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div 42, r1, stack-[1], r1
  %res = udiv i256 42, %rs1
  %rem = urem i256 42, %rs1
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: udivremxrsr
define i256 @udivremxrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s 42, r1, stack-[1], r1
  %res = udiv i256 %rs1, 42
  %rem = urem i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: udivremcrsr
define i256 @udivremcrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div @val[0], r1, stack-[1], r1
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %val, %rs1
  %rem = urem i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: udivremyrsr
define i256 @udivremyrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s @val[0], r1, stack-[1], r1
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %rs1, %val
  %rem = urem i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: udivremsrsr
define i256 @udivremsrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: div stack-[2], r1, stack-[1], r1
  %val = load i256, i256* %valptr
  %res = udiv i256 %val, %rs1
  %rem = urem i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret i256 %rem
}

; CHECK-LABEL: udivremzrsr
define i256 @udivremzrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: div.s stack-[2], r1, stack-[1], r1
  %val = load i256, i256* %valptr
  %res = udiv i256 %rs1, %val
  %rem = urem i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret i256 %rem
}

