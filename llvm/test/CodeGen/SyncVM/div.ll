; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: remrrr
define i256 @remrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: div r1, r2, r{{[0-9]+}}, r1
  %res = urem i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: remirr
define i256 @remirr(i256 %rs1) nounwind {
; CHECK: div 42, r1, r{{[0-9]+}}, r1
  %res = urem i256 42, %rs1
  ret i256 %res
}

; CHECK-LABEL: remxrr
define i256 @remxrr(i256 %rs1) nounwind {
; CHECK: div.s 42, r1, r{{[0-9]+}}, r1
  %res = urem i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: remcrr
define i256 @remcrr(i256 %rs1) nounwind {
; CHECK: div @val[0], r1, r{{[0-9]+}}, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = urem i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: remyrr
define i256 @remyrr(i256 %rs1) nounwind {
; CHECK: div.s @val[0], r1, r{{[0-9]+}}, r1
  %val = load i256, i256 addrspace(4)* @val
  %res = urem i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: remsrr
define i256 @remsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div stack-[1], r1, r{{[0-9]+}}, r1
  %val = load i256, i256* %valptr
  %res = urem i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: remzrr
define i256 @remzrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s stack-[1], r1, r{{[0-9]+}}, r1
  %val = load i256, i256* %valptr
  %res = urem i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: remrrs
define void @remrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = urem i256 %rs1, %rs2
; CHECK: div r1, r2, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: remirs
define void @remirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div 42, r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %res = urem i256 42, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: remxrs
define void @remxrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s 42, r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %res = urem i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: remcrs
define void @remcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div @val[0], r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = urem i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: remyrs
define void @remyrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s @val[0], r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res = urem i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: remsrs
define void @remsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: div stack-[2], r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %val = load i256, i256* %valptr
  %res = urem i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: remzrs
define void @remzrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; CHECK: div.s stack-[2], r1, r{{[0-9]+}}, r[[REG:[0-9]+]]
; CHECK: add r[[REG]], r0, stack-[1]
  %val = load i256, i256* %valptr
  %res = urem i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: divrrr
define i256 @divrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: div r1, r2, r1, r{{[0-9]+}}
  %res = udiv i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: divirr
define i256 @divirr(i256 %rs1) nounwind {
; CHECK: div 42, r1, r1, r{{[0-9]+}}
  %res = udiv i256 42, %rs1
  ret i256 %res
}

; CHECK-LABEL: divxrr
define i256 @divxrr(i256 %rs1) nounwind {
; CHECK: div.s 42, r1, r1, r{{[0-9]+}}
  %res = udiv i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: divcrr
define i256 @divcrr(i256 %rs1) nounwind {
; CHECK: div @val[0], r1, r1, r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: divyrr
define i256 @divyrr(i256 %rs1) nounwind {
; CHECK: div.s @val[0], r1, r1, r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: divsrr
define i256 @divsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div stack-[1], r1, r1, r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = udiv i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: divzrr
define i256 @divzrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div.s stack-[1], r1, r1, r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = udiv i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: divrrs
define void @divrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = udiv i256 %rs1, %rs2
; TODO: div r1, r2, stack-[1], r{{[0-9]+}}
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: divirs
define void @divirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: div 42, r1, stack-[1], r{{[0-9]+}}
  %res = udiv i256 42, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: divxrs
define void @divxrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: div.s 42, r1, stack-[1], r{{[0-9]+}}
  %res = udiv i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: divcrs
define void @divcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: div @val[0], r1, stack-[1], r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: divyrs
define void @divyrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: div.s @val[0], r1, stack-[1], r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: divsrs
define void @divsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; TODO: div stack-[2], r1, stack-[1], r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = udiv i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: divzrs
define void @divzrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; TODO: div.s stack-[2], r1, stack-[1], r{{[0-9]+}}
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
; TODO: div r1, r2, stack-[1], r1
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: udivremirsr
define i256 @udivremirsr(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: div 42, r1, stack-[1], r1
  %res = udiv i256 42, %rs1
  %rem = urem i256 42, %rs1
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: udivremxrsr
define i256 @udivremxrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: div.s 42, r1, stack-[1], r1
  %res = udiv i256 %rs1, 42
  %rem = urem i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: udivremcrsr
define i256 @udivremcrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: div @val[0], r1, stack-[1], r1
  %val = load i256, i256 addrspace(4)* @val
  %res = udiv i256 %val, %rs1
  %rem = urem i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: udivremyrsr
define i256 @udivremyrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: div.s @val[0], r1, stack-[1], r1
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
; TODO: div stack-[2], r1, stack-[1], r1
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
; TODO: div.s stack-[2], r1, stack-[1], r1
  %val = load i256, i256* %valptr
  %res = udiv i256 %rs1, %val
  %rem = urem i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret i256 %rem
}
