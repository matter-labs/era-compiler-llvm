; RUN: opt -passes=evm-mark-recursive-functions -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

; CHECK: define i256 @indirect_recursive2(i256 %x) #[[RECURSIVE:[0-9]+]] {
define i256 @indirect_recursive2(i256 %x) {
entry:
  %call = call i256 @indirect_recursive1(i256 %x)
  ret i256 %call
}

; CHECK: define i256 @indirect_recursive1(i256 %y) #[[RECURSIVE:[0-9]+]] {
define i256 @indirect_recursive1(i256 %y) {
entry:
  %call = call i256 @indirect_recursive2(i256 %y)
  ret i256 %call
}

; CHECK: define i256 @recursive(i256 %z) #[[RECURSIVE:[0-9]+]] {
define i256 @recursive(i256 %z) {
entry:
  %call = call i256 @recursive(i256 %z)
  ret i256 %call
}

; CHECK: define i256 @calls_recursive(i256 %a) {
define i256 @calls_recursive(i256 %a) {
entry:
  %call = call i256 @recursive(i256 %a)
  ret i256 %call
}

; CHECK: define i256 @non_recursive() {
define i256 @non_recursive() {
  ret i256 1
}

; CHECK: attributes #[[RECURSIVE]] = { "evm-recursive" }
