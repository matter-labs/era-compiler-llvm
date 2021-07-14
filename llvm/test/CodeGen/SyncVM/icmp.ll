; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: slt0
define i256 @slt0(i8 %par) nounwind {
  %1 = icmp slt i8 %par, 0
  %2 = zext i1 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: sgt0
define i256 @sgt0(i8 %par) nounwind {
  %1 = icmp sgt i8 %par, 0
  %2 = zext i1 %1 to i256
  ret i256 %2
}
