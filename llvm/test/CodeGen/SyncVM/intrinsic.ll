; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42
@val2 = addrspace(4) global i256 43

; CHECK-LABEL: stacksave
define i8* @stacksave() {
  %res = call i8* @llvm.stacksave()
; CHECK: context.sp r1
  ret i8* %res
}

; CHECK-LABEL: stackrestore
define void @stackrestore(i8* %ptr) {
; CHECK: context.sp r2
; CHECK-NEXT: sub r1, r2, r1
; CHECK-NEXT: nop stack+=[r1]
  call void @llvm.stackrestore(i8* %ptr)
  ret void
}

; CHECK-LABEL: thisr
define i256 @thisr() {
; CHECK: context.this r1
  %res = call i256 @llvm.syncvm.this()
  ret i256 %res
}

; CHECK-LABEL: callerr
define i256 @callerr() {
; CHECK: context.caller r1
  %res = call i256 @llvm.syncvm.caller()
  ret i256 %res
}

; CHECK-LABEL: codesourcer
define i256 @codesourcer() {
; CHECK: context.code_source r1
  %res = call i256 @llvm.syncvm.codesource()
  ret i256 %res
}

; CHECK-LABEL: metar
define i256 @metar() {
; CHECK: context.meta r1
  %res = call i256 @llvm.syncvm.meta()
  ret i256 %res
}

; CHECK-LABEL: txoriginr
define i256 @txoriginr() {
; CHECK: context.tx_origin r1
  %res = call i256 @llvm.syncvm.txorigin()
  ret i256 %res
}

; CHECK-LABEL: ergsleftr
define i256 @ergsleftr() {
; CHECK: context.ergs_left r1
  %res = call i256 @llvm.syncvm.ergsleft()
  ret i256 %res
}

; CHECK-LABEL: getu128
define i256 @getu128() {
; CHECK: context.get_context_u128 r1
  %res = call i256 @llvm.syncvm.getu128()
  ret i256 %res
}

; CHECK-LABEL: setu128
define void @setu128(i256 %val) {
; CHECK: context.set_context_u128 r1
  call void @llvm.syncvm.setu128(i256 %val)
  ret void
}

; CHECK-LABEL: inctx
define void @inctx() {
; CHECK: context.inc_tx_num
  call void @llvm.syncvm.inctx()
  ret void
}

; CHECK-LABEL: setprice
define void @setprice(i256 %p) {
; CHECK: context.set_ergs_per_pubdata
  call void @llvm.syncvm.setpubdataprice(i256 %p)
  ret void
}

; CHECK-LABEL: sload_rr
define i256 @sload_rr(i256 %val) {
; CHECK: sload r1, r{{[0-9]+}}
  %1 = call i256 @llvm.syncvm.sload(i256 %val)
  ret i256 %1
}

; CHECK-LABEL: sstore_r
define void @sstore_r(i256 %key, i256 %val) {
; CHECK: sstore r1, r2
  call void @llvm.syncvm.sstore(i256 %key, i256 %val)
  ret void
}

; CHECK-LABEL: tol1_r
define void @tol1_r(i256 %key, i256 %val) {
; CHECK: to_l1 r1, r2
; CHECK: to_l1.first r1, r2
  call void @llvm.syncvm.tol1(i256 %key, i256 %val, i256 0)
  call void @llvm.syncvm.tol1(i256 %key, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: event_r
define void @event_r(i256 %key, i256 %val) {
; CHECK: event r1, r2
; CHECK: event.first r1, r2
  call void @llvm.syncvm.event(i256 %key, i256 %val, i256 0)
  call void @llvm.syncvm.event(i256 %key, i256 %val, i256 1)
  ret void
}

; CHECK-LABEL: precompile_rr
define i256 @precompile_rr(i256 %key, i256 %ergs) {
; CHECK: precompile r1, r2
  %res = call i256 @llvm.syncvm.precompile(i256 %key, i256 %ergs)
  ret i256 %res
}

; CHECK-LABEL: throw
define void @throw(i256 %val) {
; CHECK: revert
  call void @llvm.syncvm.throw(i256 %val)
  ret void
}

; CHECK-LABEL: far_return
define void @far_return(i256 %x, i256 %val) {
; CHECK: add r2, r0, r1
; CHECK: ret.ok.to_label r1, @DEFAULT_FAR_RETURN
  call void @llvm.syncvm.return(i256 %val)
  unreachable
}

; CHECK-LABEL: far_revert
define void @far_revert(i256 %x, i256 %val) {
; CHECK: add r2, r0, r1
; CHECK: ret.revert.to_label r1, @DEFAULT_FAR_REVERT
  call void @llvm.syncvm.revert(i256 %val)
  unreachable
}

; CHECK-LABEL: ifeqrr
define i256 @ifeqrr(i256 %x, i256 %y) {
  ; CHECK: add.ne r2, r0, r1
  %res = call i256 @llvm.syncvm.ifeq(i256 %x, i256 %y)
  ret i256 %res
}

; CHECK-LABEL: ifeqii
define i256 @ifeqii() {
  ; CHECK: add 42, r0, r1
  ; CHECK: add.eq 0, r0, r1
  %res = call i256 @llvm.syncvm.ifeq(i256 0, i256 42)
  ret i256 %res
}

; CHECK-LABEL: ifltrr
define i256 @ifltrr(i256 %x, i256 %y) {
  ; CHECK: add.ge r2, r0, r1
  %res = call i256 @llvm.syncvm.iflt(i256 %x, i256 %y)
  ret i256 %res
}

; CHECK-LABEL: ifltii
define i256 @ifltii() {
  ; CHECK: add 42, r0, r1
  ; CHECK: add.lt 0, r0, r1
  %res = call i256 @llvm.syncvm.iflt(i256 0, i256 42)
  ret i256 %res
}

; CHECK-LABEL: ifgtrr
define i256 @ifgtrr(i256 %x, i256 %y) {
  ; CHECK: add.le r2, r0, r1
  %res = call i256 @llvm.syncvm.ifgt(i256 %x, i256 %y)
  ret i256 %res
}

; CHECK-LABEL: ifgtii
define i256 @ifgtii() {
  ; CHECK: add 42, r0, r1
  ; CHECK: add.gt 0, r0, r1
  %res = call i256 @llvm.syncvm.ifgt(i256 0, i256 42)
  ret i256 %res
}

define void @invoke.farcall({i256, i1}* %res) {
  call {i256, i1}* @__farcall(i256 0, i256 0, {i256, i1}* %res)
  ret void
}

declare i8* @llvm.stacksave()
declare void @llvm.stackrestore(i8*)

declare i256 @llvm.syncvm.this()
declare i256 @llvm.syncvm.caller()
declare i256 @llvm.syncvm.codesource()
declare i256 @llvm.syncvm.meta()
declare i256 @llvm.syncvm.txorigin()
declare i256 @llvm.syncvm.ergsleft()
declare i256 @llvm.syncvm.getu128()
declare void @llvm.syncvm.setu128(i256)
declare void @llvm.syncvm.inctx()
declare void @llvm.syncvm.setpubdataprice(i256)
declare i256 @llvm.syncvm.sload(i256)
declare void @llvm.syncvm.sstore(i256, i256)
declare void @llvm.syncvm.tol1(i256, i256, i256)
declare void @llvm.syncvm.event(i256, i256, i256)
declare i256 @llvm.syncvm.precompile(i256, i256)
declare void @llvm.syncvm.throw(i256)
declare void @llvm.syncvm.return(i256)
declare void @llvm.syncvm.revert(i256)
declare i256 @llvm.syncvm.ifeq(i256, i256)
declare i256 @llvm.syncvm.iflt(i256, i256)
declare i256 @llvm.syncvm.ifgt(i256, i256)

declare {i256, i1}* @__farcall(i256, i256, {i256, i1}*)
