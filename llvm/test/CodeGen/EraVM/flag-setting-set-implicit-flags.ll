; RUN: llc -opaque-pointers -march=eravm -stop-after=eravm-combine-flag-setting -simplify-mir < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42
declare void @foo(i256 %val)

; CHECK: name: ADDrrr_v
define i1 @ADDrrr_v(i256 %p1, i256 %p2) nounwind {
  %p3 = add i256 %p1, %p2
; CHECK: ADDrrr_v %{{[0-9]+}}, %{{[0-9]+}}, 0, implicit-def $flags
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK: name: ADDsrr_v
define i1 @ADDsrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: ADDsrr_v %stack.0.valptr, i256 0, 0, %0, 0, implicit-def $flags
  %p2 = add i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK: name: ADDcrr_v
define i1 @ADDcrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: ADDcrr_v i256 0, @val, %0, 0, implicit-def $flags
  %p2 = add i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}
