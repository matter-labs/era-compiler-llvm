; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:16:8-i256:256:256"
target triple = "tvm"

declare i256 @onearg(i256 %a1) nounwind
declare i256 @twoarg(i256 %a1, i256 %a2) nounwind
declare i256 @threearg(i256 %a1, i256 %a2, i256 %a3) nounwind

; CHECK-LABEL: caller1
define i256 @caller1(i256 %a1) nounwind {
; CHECK: call @onearg
  %1 = call i256 @onearg(i256 %a1)
  ret i256 %1
}

; CHECK-LABEL: caller2
define i256 @caller2(i256 %a1, i256 %a2) nounwind {
; CHECK: call @twoarg
  %1 = call i256 @twoarg(i256 %a1, i256 %a2)
  ret i256 %1
}

; CHECK-LABEL: caller3
define i256 @caller3(i256 %a1, i256 %a2, i256 %a3) nounwind {
; CHECK: call @threearg
  %1 = call i256 @threearg(i256 %a1, i256 %a2, i256 %a3)
  ret i256 %1
}
