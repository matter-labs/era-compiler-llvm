; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: mulrrr
define i256 @mulrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK: mul r1, r2, r1, r{{[0-9]+}}
  %res = mul i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: mulirr
define i256 @mulirr(i256 %rs1) nounwind {
; CHECK: mul 42, r1, r1, r{{[0-9]+}}
  %res = mul i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: mulcrr
define i256 @mulcrr(i256 %rs1) nounwind {
; CHECK: mul @val[0], r1, r1, r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = mul i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: mulsrr
define i256 @mulsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: mul stack-[1], r1, r1, r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = mul i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: mulrrs
define void @mulrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = mul i256 %rs1, %rs2
; TODO: mul r1, r2, stack-[1], r{{[0-9]+}}
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: mulirs
define void @mulirs(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: mul 42, r1, stack-[1], r{{[0-9]+}}
  %res = mul i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: mulcrs
define void @mulcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: mul @val[0], r1, stack-[1], r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = mul i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void 
}

; CHECK-LABEL: mulsrs
define void @mulsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; TODO: mul stack-[2], r1, stack-[1], r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = mul i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: umullohirrrr
define i256 @umullohirrrr(i256 %rs1, i256 %rs2) nounwind {
  %rs1l = zext i256 %rs1 to i512
  %rs2l = zext i256 %rs2 to i512
; CHECK: mul r1, r2, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
  %resl = mul i512 %rs1l, %rs2l
  %res1 = trunc i512 %resl to i256
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
; CHECK: add r[[REG1]], r[[REG2]], r1
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: mulhiirr
define i256 @mulhiirr(i256 %rs1) nounwind {
  %rs1l = zext i256 %rs1 to i512
; CHECK: mul 42, r1, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
  %resl = mul i512 %rs1l, 42
  %res1 = trunc i512 %resl to i256
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
; CHECK: add r[[REG1]], r[[REG2]], r1
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: mulhicrr
define i256 @mulhicrr(i256 %rs1) nounwind {
  %rs1l = zext i256 %rs1 to i512
  %val = load i256, i256 addrspace(4)* @val
  %rs2l = zext i256 %val to i512
; CHECK: mul @val[0], r1, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
  %resl = mul i512 %rs1l, %rs2l
  %res1 = trunc i512 %resl to i256
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
; CHECK: add r[[REG1]], r[[REG2]], r1
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: mulhisrr
define i256 @mulhisrr(i256 %rs1) nounwind {
  %rs1l = zext i256 %rs1 to i512
  %valptr = alloca i256
; CHECK: mul stack-[1], r1, r[[REG1:[0-9]+]], r[[REG2:[0-9]+]]
  %val = load i256, i256* %valptr
  %rs2l = zext i256 %val to i512
  %resl = mul i512 %rs2l, %rs1l
  %res1 = trunc i512 %resl to i256
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
; CHECK: add r[[REG1]], r[[REG2]], r1
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: umullohirrsr
define i256 @umullohirrsr(i256 %rs1, i256 %rs2) nounwind {
  %rs1l = zext i256 %rs1 to i512
  %rs2l = zext i256 %rs2 to i512
  %valptr = alloca i256
  %resl = mul i512 %rs1l, %rs2l
; TODO: mul r1, r2, stack-[1], r1
  %res1 = trunc i512 %resl to i256
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
  store i256 %res1, i256* %valptr
  ret i256 %res2
}

; CHECK-LABEL: umullohiirsr
define i256 @umullohiirsr(i256 %rs1) nounwind {
  %rs1l = zext i256 %rs1 to i512
  %valptr = alloca i256
; TODO: mul 42, r1, stack-[1], r1
  %resl = mul i512 %rs1l, 42
  %res1 = trunc i512 %resl to i256
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
  store i256 %res1, i256* %valptr
  ret i256 %res2
}

; CHECK-LABEL: umullohicrsr
define i256 @umullohicrsr(i256 %rs1) nounwind {
  %rs1l = zext i256 %rs1 to i512
  %valptr = alloca i256
; TODO: mul @val[0], r1, stack-[1], r1
  %val = load i256, i256 addrspace(4)* @val
  %rs2l = zext i256 %val to i512
  %resl = mul i512 %rs1l, %rs2l
  %res1 = trunc i512 %resl to i256
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
  store i256 %res1, i256* %valptr
  ret i256 %res2
}

; CHECK-LABEL: umullohisrsr
define i256 @umullohisrsr(i256 %rs1) nounwind {
  %rs1l = zext i256 %rs1 to i512
  %valptr = alloca i256
  %destptr = alloca i256
; TODO: mul stack-[2], r1, stack-[1], r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %rs2l = zext i256 %val to i512
  %resl = mul i512 %rs2l, %rs1l
  %res1 = trunc i512 %resl to i256
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
  store i256 %res1, i256* %destptr
  ret i256 %res2
}

; CHECK-LABEL: mulhs
define i256 @mulhs (i256 %a, i256 %b) nounwind {
; just to check that we support expanding mulhs
  %1 = sext i256 %a to i512
  %2 = sext i256 %b to i512
  %3 = mul i512 %1, %2
  %4 = lshr i512 %3, 256
  %5 = trunc i512 %4 to i256
  ret i256 %5
}

; CHECK-LABEL: mulhu
define i256 @mulhu (i256 %a, i256 %b) nounwind {
  %1 = zext i256 %a to i512
  %2 = zext i256 %b to i512
  %3 = mul i512 %1, %2
  %4 = lshr i512 %3, 256
  %5 = trunc i512 %4 to i256
; MULHU expands into UMUL_LOHI
; CHECK: mul r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  ret i256 %5
}

; CHECK-LABEL: mulhu_imm
define i256 @mulhu_imm (i256 %a) nounwind {
  %1 = zext i256 %a to i512
  %2 = mul i512 %1, 12345
; TODO: Enable this when stack instruction is generated, as well as
; stack and calldata addressing flavor tests
; TODO: mul 12345, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %3 = lshr i512 %2, 256
  %4 = trunc i512 %3 to i256
  ret i256 %4
}

