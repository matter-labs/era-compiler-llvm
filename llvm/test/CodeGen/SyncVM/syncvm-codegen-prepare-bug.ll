; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: syncvm-codegenprepare-bug
define i64 @syncvm-codegenprepare-bug() {
entry:
  %0 = and i64 undef, 255
  ret i64 %0
}
