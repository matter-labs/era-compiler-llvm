; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42
@ptr = global i8 addrspace(3)* null

; CHECK-LABEL: ptraddrrr
define i8 addrspace(3)* @ptraddrrr(i8 addrspace(3)* %rs1, i256 %rs2) nounwind {
; CHECK: addp r1, r2, r1
  %res1 = getelementptr i8, i8 addrspace(3)* %rs1, i256 %rs2
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddrir
define i8 addrspace(3)* @ptraddrir(i8 addrspace(3)* %rs1) nounwind {
; CHECK: addp.s 42, r1, r1
  %res1 = getelementptr i8, i8 addrspace(3)* %rs1, i256 42
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptrsubrir
define i8 addrspace(3)* @ptrsubrir(i8 addrspace(3)* %rs1) nounwind {
; CHECK: subp.s 1, r1, r1
  %res1 = getelementptr i8, i8 addrspace(3)* %rs1, i256 -1
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddrsr
define i8 addrspace(3)* @ptraddrsr(i8 addrspace(3)* %rs1) nounwind {
  %valptr = alloca i256
; CHECK: addp.s stack-[1], r1, r1
  %val = load i256, i256* %valptr
  %res1 = getelementptr i8, i8 addrspace(3)* %rs1, i256 %val
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddrcr
define i8 addrspace(3)* @ptraddrcr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: addp.s code[@val], r1, r1
  %val = load i256, i256 addrspace(4)* @val
  %res1 = getelementptr i8, i8 addrspace(3)* %rs1, i256 %val
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddgrr
define i8 addrspace(3)* @ptraddgrr(i256 %rs2) nounwind {
; CHECK: addp stack[@ptr], r1, r1
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %rs2
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddgir
define i8 addrspace(3)* @ptraddgir(i8 addrspace(3)* %rs1) nounwind {
; CHECK: add 42, r0, r1
; CHECK: addp stack[@ptr], r1, r1
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 42
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddgsr
define i8 addrspace(3)* @ptraddgsr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: add stack-[1], r0, r1
; CHECK: addp stack[@ptr], r1, r1
  %valptr = alloca i256
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256* %valptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddgcr
define i8 addrspace(3)* @ptraddgcr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: add code[@val], r0, r1
; CHECK: addp stack[@ptr], r1, r1
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddsrr
define i8 addrspace(3)* @ptraddsrr(i256 %rs2) nounwind {
; CHECK: addp stack-[1], r1, r1
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %rs2
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddsir
define i8 addrspace(3)* @ptraddsir(i8 addrspace(3)* %rs1) nounwind {
; CHECK: add 42, r0, r1
; CHECK: addp stack-[1], r1, r1
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 42
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddssr
define i8 addrspace(3)* @ptraddssr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: add stack-[2], r0, r1
; CHECK: addp stack-[1], r1, r1
  %valptr = alloca i256
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256* %valptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddscr
define i8 addrspace(3)* @ptraddscr(i8 addrspace(3)* %rs1) nounwind {
; CHECK: add code[@val], r0, r1
; CHECK: addp stack-[1], r1, r1
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  ret i8 addrspace(3)* %res1
}

; CHECK-LABEL: ptraddrrs
define void @ptraddrrs(i8 addrspace(3)* %rs1, i256 %rs2) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: addp r1, r2, stack-[1]
  %res1 = getelementptr i8, i8 addrspace(3)* %rs1, i256 %rs2
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddris
define void @ptraddris(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: addp.s 42, r1, stack-[1]
  %res1 = getelementptr i8, i8 addrspace(3)* %rs1, i256 42
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptrsubris
define void @ptrsubris(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: subp.s 1, r1, stack-[1]
  %res1 = getelementptr i8, i8 addrspace(3)* %rs1, i256 -1
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddrss
define void @ptraddrss(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
  %valptr = alloca i256
; CHECK: addp.s stack-[1], r1, stack-[2]
  %val = load i256, i256* %valptr
  %res1 = getelementptr i8, i8 addrspace(3)* %rs1, i256 %val
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddrcs
define void @ptraddrcs(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: addp.s code[@val], r1, stack-[1]
  %val = load i256, i256 addrspace(4)* @val
  %res1 = getelementptr i8, i8 addrspace(3)* %rs1, i256 %val
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddgrs
define void @ptraddgrs(i256 %rs2) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: addp stack[@ptr], r1, stack-[1]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %rs2
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddgis
define void @ptraddgis(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
  ; CHECK: add 42, r0, r1
  ; CHECK: addp stack[@ptr], r1, stack-[1]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 42
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddgss
define void @ptraddgss(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: add stack-[1], r0, r1
; CHECK: addp stack[@ptr], r1, stack-[2]
  %valptr = alloca i256
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256* %valptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddgcs
define void @ptraddgcs(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
  ; CHECK: add code[@val], r0, r1
  ; CHECK: addp stack[@ptr], r1, stack-[1]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddsrs
define void @ptraddsrs(i256 %rs2) nounwind {
  %result = alloca i8 addrspace(3)*
; CHECK: addp stack-[1], r1, stack-[2]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %rs2
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddsis
define void @ptraddsis(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
  ; CHECK: add 42, r0, r1
  ; CHECK: addp stack-[1], r1, stack-[2]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 42
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddsss
define void @ptraddsss(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
  ; CHECK: add stack-[2], r0, r1
  ; CHECK: addp stack-[1], r1, stack-[3]
  %valptr = alloca i256
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256* %valptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddscs
define void @ptraddscs(i8 addrspace(3)* %rs1) nounwind {
  %result = alloca i8 addrspace(3)*
  ; CHECK: add code[@val], r0, r1
  ; CHECK: addp stack-[1], r1, stack-[2]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  store i8 addrspace(3)* %res1, i8 addrspace(3)** %result
  ret void
}

; CHECK-LABEL: ptraddgrg
define void @ptraddgrg(i256 %rs2) nounwind {
; CHECK: addp stack[@ptr], r1, stack[@ptr]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %rs2
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptraddgig
define void @ptraddgig(i8 addrspace(3)* %rs1) nounwind {
  ; CHECK: add 42, r0, r1
  ; CHECK: addp stack[@ptr], r1, stack[@ptr]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 42
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptraddgsg
define void @ptraddgsg(i8 addrspace(3)* %rs1) nounwind {
  ; CHECK: add stack-[1], r0, r1
  ; CHECK: addp stack[@ptr], r1, stack[@ptr]
  %valptr = alloca i256
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256* %valptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptraddgcg
define void @ptraddgcg(i8 addrspace(3)* %rs1) nounwind {
; CHECK: add code[@val], r0, r1
; CHECK: addp stack[@ptr], r1, stack[@ptr]
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** @ptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptraddsrg
define void @ptraddsrg(i256 %rs2) nounwind {
; CHECK: addp stack-[1], r1, stack[@ptr]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %rs2
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptraddsig
define void @ptraddsig(i8 addrspace(3)* %rs1) nounwind {
  ; CHECK: add 42, r0, r1
  ; CHECK: addp stack-[1], r1, stack[@ptr]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 42
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptraddssg
define void @ptraddssg(i8 addrspace(3)* %rs1) nounwind {
  ; CHECK: add stack-[2], r0, r1
  ; CHECK: addp stack-[1], r1, stack[@ptr]
  %valptr = alloca i256
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256* %valptr
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

; CHECK-LABEL: ptraddscg
define void @ptraddscg(i8 addrspace(3)* %rs1) nounwind {
  ; CHECK: add code[@val], r0, r1
  ; CHECK: addp stack-[1], r1, stack[@ptr]
  %ptrptr = alloca i8 addrspace(3)*
  %ptr = load i8 addrspace(3)*, i8 addrspace(3)** %ptrptr
  %val = load i256, i256 addrspace(4)* @val
  %res1 = getelementptr i8, i8 addrspace(3)* %ptr, i256 %val
  store i8 addrspace(3)* %res1, i8 addrspace(3)** @ptr
  ret void
}

declare i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)*, i256)
