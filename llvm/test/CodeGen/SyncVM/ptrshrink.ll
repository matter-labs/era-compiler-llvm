; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42
@ptr = global i8 addrspace(3)* null

; CHECK-LABEL: ptrshrinkrrr
define i8 addrspace(3)* @ptrshrinkrrr(i8 addrspace(3)* %rs1, i256 %rs2) nounwind {
; CHECK: ptr.shrink r1, r2, r1
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %rs1, i256 %rs2)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinkrir
define i8 addrspace(3)* @ptrshrinkrir(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.shrink.s 42, r1, r1
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %rs1, i256 42)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinkrsr
define i8 addrspace(3)* @ptrshrinkrsr(i8 addrspace(3)* %rs1) nounwind {
  %valptr = alloca i256
; CHECK: ptr.shrink.s stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %rs1, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinkrcr
define i8 addrspace(3)* @ptrshrinkrcr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.shrink.s @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %rs1, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinkgrr
define i8 addrspace(3)* @ptrshrinkgrr(i256 %rs2) nounwind {
; TODO: should be ptr.shrink stack[@ptr], r0, r1
; CHECK: ptr.add stack[@ptr], r0, r2
; CHECK: ptr.shrink r2, r1, r1
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %rs2)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinkgir
define i8 addrspace(3)* @ptrshrinkgir(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; CHECK: ptr.shrink.s 42, r1, r1
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 42)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinkgsr
define i8 addrspace(3)* @ptrshrinkgsr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; CHECK: ptr.shrink.s stack-[1], r1, r1
  %valptr = alloca i256
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinkgcr
define i8 addrspace(3)* @ptrshrinkgcr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; CHECK: ptr.shrink.s @val[0], r1, r1
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinksrr
define i8 addrspace(3)* @ptrshrinksrr(i256 %rs2) nounwind {
; TODO: should be ptr.shrink stack-[1], r0, r1
; CHECK: ptr.add stack-[1], r0, r2
; CHECK: ptr.shrink r2, r1, r1
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %rs2)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinksir
define i8 addrspace(3)* @ptrshrinksir(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; CHECK: ptr.shrink.s 42, r1, r1
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 42)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinkssr
define i8 addrspace(3)* @ptrshrinkssr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; CHECK: ptr.shrink.s stack-[2], r1, r1
  %valptr = alloca i256
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinkscr
define i8 addrspace(3)* @ptrshrinkscr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; CHECK: ptr.shrink.s @val[0], r1, r1
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrshrinkrrs
define void @ptrshrinkrrs(i8 addrspace(3)* %rs1, i256 %rs2) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: Should be ptr.shrink r1, r2, stack-[1]
; CHECK: ptr.shrink r1, r2, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %rs1, i256 %rs2)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinkris
define void @ptrshrinkris(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: Should be ptr.shrink.s 42, r1, stack-[1]
; CHECK: ptr.shrink.s 42, r1, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %rs1, i256 42)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinkrss
define void @ptrshrinkrss(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
  %valptr = alloca i256
; TODO: Should be ptr.shrink.s stack-[1], r1, stack-[2]
; CHECK: ptr.shrink.s stack-[1], r1, r1
; CHECK: ptr.add r1, r0, stack-[2]
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %rs1, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinkrcs
define void @ptrshrinkrcs(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: Should be ptr.shrink.s @val[0], r1, stack-[1]
; CHECK: ptr.shrink.s @val[0], r1, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %rs1, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinkgrs
define void @ptrshrinkgrs(i256 %rs2) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: should be ptr.shrink stack[@ptr], r1, stack-[1]
; CHECK: ptr.add stack[@ptr], r0, r2
; CHECK: ptr.shrink r2, r1, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %rs2)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinkgis
define void @ptrshrinkgis(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: should be ptr.shrink.s 42, r1, stack-[1]
; CHECK: ptr.shrink.s 42, r1, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %rs1, i256 42)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinkgss
define void @ptrshrinkgss(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: ptr.add stack[@ptr], r0, r1
; TODO: should be ptr.shrink.s stack-[1], r1, stack-[2]
; CHECK: ptr.shrink.s stack-[1], r1, r1
; CHECK: ptr.add r1, r0, stack-[2]
  %valptr = alloca i256
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinkgcs
define void @ptrshrinkgcs(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: ptr.add stack[@ptr], r0, r1
; TODO: should be ptr.shrink.s @val[0], r1, stack-[1]
; CHECK: ptr.shrink.s @val[0], r1, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinksrs
define void @ptrshrinksrs(i256 %rs2) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: should be ptr.shrink stack-[1], r0, stack-[2]
; CHECK: ptr.add stack-[1], r0, r2
; CHECK: ptr.shrink r2, r1, r1
; CHECK: ptr.add r1, r0, stack-[2]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %rs2)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinksis
define void @ptrshrinksis(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.shrink.s 42, r1, stack-[2]
; CHECK: ptr.shrink.s 42, r1, r1
; CHECK: ptr.add r1, r0, stack-[2]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 42)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinksss
define void @ptrshrinksss(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.shrink.s stack-[2], r1, stack-[3]
; CHECK: ptr.shrink.s stack-[2], r1, r1
; CHECK: ptr.add r1, r0, stack-[3]
  %valptr = alloca i256
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinkscs
define void @ptrshrinkscs(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.shrink.s @val[0], r1, stack-[2]
; CHECK: ptr.shrink.s @val[0], r1, r1
; CHECK: ptr.add r1, r0, stack-[2]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrshrinkgrg
define void @ptrshrinkgrg(i256 %rs2) nounwind {
; TODO: should be ptr.shrink stack[@ptr], r1, stack[@ptr]
; CHECK: ptr.add stack[@ptr], r0, r2
; CHECK: ptr.shrink r2, r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %rs2)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrshrinkgig
define void @ptrshrinkgig(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; TODO: should be ptr.shrink.s 42, r1, stack[@ptr]
; CHECK: ptr.shrink.s 42, r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 42)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrshrinkgsg
define void @ptrshrinkgsg(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; TODO: should be ptr.shrink.s stack-[1], r1, stack[@ptr]
; CHECK: ptr.shrink.s stack-[1], r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %valptr = alloca i256
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrshrinkgcg
define void @ptrshrinkgcg(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; TODO: should be ptr.shrink.s @val[0], r1, stack[@ptr]
; CHECK: ptr.shrink.s @val[0], r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrshrinksrg
define void @ptrshrinksrg(i256 %rs2) nounwind {
; TODO: should be ptr.shrink stack-[1], r0, stack[@ptr]
; CHECK: ptr.add stack-[1], r0, r2
; CHECK: ptr.shrink r2, r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %rs2)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrshrinksig
define void @ptrshrinksig(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.shrink.s 42, r1, stack[@ptr]
; CHECK: ptr.shrink.s 42, r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 42)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrshrinkssg
define void @ptrshrinkssg(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.shrink.s stack-[2], r1, stack[@ptr]
; CHECK: ptr.shrink.s stack-[2], r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %valptr = alloca i256
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrshrinkscg
define void @ptrshrinkscg(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.shrink.s @val[0], r1, stack[@ptr]
; CHECK: ptr.shrink.s @val[0], r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

declare i8 addrspace(3)* @llvm.syncvm.ptr.shrink(i8 addrspace(3)*, i256)
