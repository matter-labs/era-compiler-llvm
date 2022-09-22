; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42
@ptr = global i8 addrspace(3)* null

; CHECK-LABEL: ptrpackrrr
define i8 addrspace(3)* @ptrpackrrr(i8 addrspace(3)* %rs1, i256 %rs2) nounwind {
; CHECK: ptr.pack r1, r2, r1
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %rs1, i256 %rs2)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpackrir
define i8 addrspace(3)* @ptrpackrir(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.pack.s 42, r1, r1
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %rs1, i256 42)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpackrsr
define i8 addrspace(3)* @ptrpackrsr(i8 addrspace(3)* %rs1) nounwind {
  %valptr = alloca i256
; CHECK: ptr.pack.s stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %rs1, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpackrcr
define i8 addrspace(3)* @ptrpackrcr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.pack.s @val[0], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %rs1, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpackgrr
define i8 addrspace(3)* @ptrpackgrr(i256 %rs2) nounwind {
; TODO: should be ptr.pack stack[@ptr], r0, r1
; CHECK: ptr.add stack[@ptr], r0, r2
; CHECK: ptr.pack r2, r1, r1
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %rs2)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpackgir
define i8 addrspace(3)* @ptrpackgir(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; CHECK: ptr.pack.s 42, r1, r1
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 42)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpackgsr
define i8 addrspace(3)* @ptrpackgsr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; CHECK: ptr.pack.s stack-[1], r1, r1
  %valptr = alloca i256
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpackgcr
define i8 addrspace(3)* @ptrpackgcr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; CHECK: ptr.pack.s @val[0], r1, r1
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpacksrr
define i8 addrspace(3)* @ptrpacksrr(i256 %rs2) nounwind {
; TODO: should be ptr.pack stack-[1], r0, r1
; CHECK: ptr.add stack-[1], r0, r2
; CHECK: ptr.pack r2, r1, r1
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %rs2)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpacksir
define i8 addrspace(3)* @ptrpacksir(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; CHECK: ptr.pack.s 42, r1, r1
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 42)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpackssr
define i8 addrspace(3)* @ptrpackssr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; CHECK: ptr.pack.s stack-[2], r1, r1
  %valptr = alloca i256
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpackscr
define i8 addrspace(3)* @ptrpackscr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; CHECK: ptr.pack.s @val[0], r1, r1
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrpackrrs
define void @ptrpackrrs(i8 addrspace(3)* %rs1, i256 %rs2) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: Should be ptr.pack r1, r2, stack-[1]
; CHECK: ptr.pack r1, r2, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %rs1, i256 %rs2)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpackris
define void @ptrpackris(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: Should be ptr.pack.s 42, r1, stack-[1]
; CHECK: ptr.pack.s 42, r1, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %rs1, i256 42)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpackrss
define void @ptrpackrss(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
  %valptr = alloca i256
; TODO: Should be ptr.pack.s stack-[1], r1, stack-[2]
; CHECK: ptr.pack.s stack-[1], r1, r1
; CHECK: ptr.add r1, r0, stack-[2]
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %rs1, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpackrcs
define void @ptrpackrcs(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: Should be ptr.pack.s @val[0], r1, stack-[1]
; CHECK: ptr.pack.s @val[0], r1, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %rs1, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpackgrs
define void @ptrpackgrs(i256 %rs2) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: should be ptr.pack stack[@ptr], r1, stack-[1]
; CHECK: ptr.add stack[@ptr], r0, r2
; CHECK: ptr.pack r2, r1, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %rs2)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpackgis
define void @ptrpackgis(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: should be ptr.pack.s 42, r1, stack-[1]
; CHECK: ptr.pack.s 42, r1, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %rs1, i256 42)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpackgss
define void @ptrpackgss(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: ptr.add stack[@ptr], r0, r1
; TODO: should be ptr.pack.s stack-[1], r1, stack-[2]
; CHECK: ptr.pack.s stack-[1], r1, r1
; CHECK: ptr.add r1, r0, stack-[2]
  %valptr = alloca i256
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpackgcs
define void @ptrpackgcs(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: ptr.add stack[@ptr], r0, r1
; TODO: should be ptr.pack.s @val[0], r1, stack-[1]
; CHECK: ptr.pack.s @val[0], r1, r1
; CHECK: ptr.add r1, r0, stack-[1]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpacksrs
define void @ptrpacksrs(i256 %rs2) nounwind {
  %result = alloca i8 addrspace(3)*
; TODO: should be ptr.pack stack-[1], r0, stack-[2]
; CHECK: ptr.add stack-[1], r0, r2
; CHECK: ptr.pack r2, r1, r1
; CHECK: ptr.add r1, r0, stack-[2]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %rs2)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpacksis
define void @ptrpacksis(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.pack.s 42, r1, stack-[2]
; CHECK: ptr.pack.s 42, r1, r1
; CHECK: ptr.add r1, r0, stack-[2]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 42)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpacksss
define void @ptrpacksss(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.pack.s stack-[2], r1, stack-[3]
; CHECK: ptr.pack.s stack-[2], r1, r1
; CHECK: ptr.add r1, r0, stack-[3]
  %valptr = alloca i256
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpackscs
define void @ptrpackscs(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.pack.s @val[0], r1, stack-[2]
; CHECK: ptr.pack.s @val[0], r1, r1
; CHECK: ptr.add r1, r0, stack-[2]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrpackgrg
define void @ptrpackgrg(i256 %rs2) nounwind {
; TODO: should be ptr.pack stack[@ptr], r1, stack[@ptr]
; CHECK: ptr.add stack[@ptr], r0, r2
; CHECK: ptr.pack r2, r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %rs2)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrpackgig
define void @ptrpackgig(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; TODO: should be ptr.pack.s 42, r1, stack[@ptr]
; CHECK: ptr.pack.s 42, r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 42)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrpackgsg
define void @ptrpackgsg(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; TODO: should be ptr.pack.s stack-[1], r1, stack[@ptr]
; CHECK: ptr.pack.s stack-[1], r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %valptr = alloca i256
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrpackgcg
define void @ptrpackgcg(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack[@ptr], r0, r1
; TODO: should be ptr.pack.s @val[0], r1, stack[@ptr]
; CHECK: ptr.pack.s @val[0], r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrpacksrg
define void @ptrpacksrg(i256 %rs2) nounwind {
; TODO: should be ptr.pack stack-[1], r0, stack[@ptr]
; CHECK: ptr.add stack-[1], r0, r2
; CHECK: ptr.pack r2, r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %rs2)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrpacksig
define void @ptrpacksig(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.pack.s 42, r1, stack[@ptr]
; CHECK: ptr.pack.s 42, r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 42)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrpackssg
define void @ptrpackssg(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.pack.s stack-[2], r1, stack[@ptr]
; CHECK: ptr.pack.s stack-[2], r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %valptr = alloca i256
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256* %valptr
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptrpackscg
define void @ptrpackscg(i8 addrspace(3)* %rs1) nounwind {
; CHECK: ptr.add stack-[1], r0, r1
; TODO: should be ptr.pack.s @val[0], r1, stack[@ptr]
; CHECK: ptr.pack.s @val[0], r1, r1
; CHECK: ptr.add r1, r0, stack[@ptr]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = call i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)* %ptr, i256 %val)
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

declare i8 addrspace(3)* @llvm.syncvm.ptr.pack(i8 addrspace(3)*, i256)
