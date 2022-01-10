; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: slt0
define i256 @slt0(i8 %par) nounwind {
  %1 = icmp slt i8 %par, 0
  %2 = zext i1 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: slt1
define i256 @slt1(i8 %par) nounwind {
  %1 = icmp slt i8 %par, 1
  %2 = zext i1 %1 to i256
  ret i256 %2
}
