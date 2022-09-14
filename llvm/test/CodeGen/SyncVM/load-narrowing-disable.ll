; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@ptr = private unnamed_addr global i256 0

; CHECK-LABEL: simplify_setcc
define i256 @simplify_setcc() {
  ; CHECK-NOT: @ptr+31
  %val = load i256, i256* @ptr, align 32
  %val.and = and i256 %val, 2
  %cmp = icmp eq i256 %val.and, 0
  br i1 %cmp, label %e2, label %e1
e1:
  ret i256 42
e2:
  unreachable
}
