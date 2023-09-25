; RUN: llc -debug-only=isel %s -o /dev/null 2>&1 | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "eravm"


; CHECK-LABEL: sdivrem
define i256 @sdivrem(i256 %rs1, i256 %rs2) nounwind {
  %res1 = srem i256 %rs1, %rs2
  %res2 = sdiv i256 %rs1, %rs2
  %res = add i256 %res1, %res2
; CHECK: i256,i256 = sdivrem t
  ret i256 %res
}

; CHECK-LABEL: sdivrem2
define i256 @sdivrem2(i256 %rs1, i256 %rs2) nounwind {
  %res1 = sdiv i256 %rs1, %rs2
  %res2 = srem i256 %rs1, %rs2
  %res = add i256 %res1, %res2
; CHECK: i256,i256 = sdivrem
  ret i256 %res
}
