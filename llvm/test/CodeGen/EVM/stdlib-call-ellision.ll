; RUN: opt -O2 -S < %s | FileCheck %s

; UNSUPPORTED: eravm

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i256 @test__addmod(i256 %arg1, i256 %arg2, i256 %modulo) {
; CHECK: @llvm.evm.addmod

  %res = call i256 @__addmod(i256 %arg1, i256 %arg2, i256 %modulo)
  ret i256 %res
}

define i256 @test__mulmod(i256 %arg1, i256 %arg2, i256 %modulo) {
; CHECK: @llvm.evm.mulmod

  %res = call i256 @__mulmod(i256 %arg1, i256 %arg2, i256 %modulo)
  ret i256 %res
}

define i256 @test__signextend(i256 %bytesize, i256 %val) {
; CHECK: @llvm.evm.signextend

  %res = call i256 @__signextend(i256 %bytesize, i256 %val)
  ret i256 %res
}

define i256 @test__exp(i256 %base, i256 %exp) {
; CHECK: @llvm.evm.exp

  %res = call i256 @__exp(i256 %base, i256 %exp)
  ret i256 %res
}

define i256 @test__byte(i256 %index, i256 %val) {
; CHECK: @llvm.evm.byte

  %res = call i256 @__byte(i256 %index, i256 %val)
  ret i256 %res
}

define i256 @test__sdiv(i256 %dividend, i256 %divisor) {
; CHECK: @llvm.evm.sdiv

  %res = call i256 @__sdiv(i256 %dividend, i256 %divisor)
  ret i256 %res
}

define i256 @test__div(i256 %dividend, i256 %divisor) {
; CHECK: @llvm.evm.div

  %res = call i256 @__div(i256 %dividend, i256 %divisor)
  ret i256 %res
}

define i256 @test__smod(i256 %val, i256 %mod) {
; CHECK: @llvm.evm.smod

  %res = call i256 @__smod(i256 %val, i256 %mod)
  ret i256 %res
}

define i256 @test__mod(i256 %val, i256 %mod) {
; CHECK: @llvm.evm.mod

  %res = call i256 @__mod(i256 %val, i256 %mod)
  ret i256 %res
}

define i256 @test__shl(i256 %shift, i256 %val) {
; CHECK: @llvm.evm.shl

  %res = call i256 @__shl(i256 %shift, i256 %val)
  ret i256 %res
}

define i256 @test__shr(i256 %shift, i256 %val) {
; CHECK: @llvm.evm.shr

  %res = call i256 @__shr(i256 %shift, i256 %val)
  ret i256 %res
}

define i256 @test__sar(i256 %shift, i256 %val) {
; CHECK: @llvm.evm.sar

  %res = call i256 @__sar(i256 %shift, i256 %val)
  ret i256 %res
}

define i256 @test__sha3(ptr addrspace(1) %offset, i256 %len) {
; CHECK: @llvm.evm.sha3

  %res = call i256 @__sha3(ptr addrspace(1) %offset, i256 %len, i1 undef)
  ret i256 %res
}

declare i256 @__addmod(i256, i256, i256) #0
declare i256 @__mulmod(i256, i256, i256) #0
declare i256 @__signextend(i256, i256) #0
declare i256 @__exp(i256, i256) #0
declare i256 @__byte(i256, i256) #0
declare i256 @__sdiv(i256, i256) #0
declare i256 @__div(i256, i256) #0
declare i256 @__smod(i256, i256) #0
declare i256 @__mod(i256, i256) #0
declare i256 @__shl(i256, i256) #0
declare i256 @__shr(i256, i256) #0
declare i256 @__sar(i256, i256) #0
declare i256 @__sha3(ptr addrspace(1), i256, i1) #1

attributes #0 = { alwaysinline mustprogress nofree norecurse nosync nounwind readnone willreturn }
attributes #1 = { alwaysinline argmemonly readonly nofree null_pointer_is_valid }
