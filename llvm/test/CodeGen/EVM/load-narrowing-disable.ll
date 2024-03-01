; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

@ptr = private unnamed_addr addrspace(1) global i256 0

define i1 @simplify_setcc() {
; CHECK-LABEL: simplify_setcc
; CHECK-NOT: @ptr+31
  %val = load i256, ptr addrspace(1) @ptr, align 32
  %val.and = and i256 %val, 2
  %cmp = icmp eq i256 %val.and, 0
  ret i1 %cmp
}
