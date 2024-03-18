; RUN: llc < %s -mtriple=eravm-unknown-unknown | FileCheck %s

@val = addrspace(4) global i256 42

define void @test1(i256 %rs1) {
  %valptr = alloca i256
  %val = load i256, i256 addrspace(4)* @val
  %res = add i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void
}

define void @test2(i256 %rs1) {
  %valptr = alloca i256
  %destptr = alloca i256
  %val = load i256, i256* %valptr
  %res = add i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}
