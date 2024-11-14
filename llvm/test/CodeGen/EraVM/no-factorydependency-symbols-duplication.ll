; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare i256 @llvm.eravm.factorydependency(metadata)

; CHECK: .wide_reloc_symbol0:
; CHECK-NOT: .wide_reloc_symbol1:

define i256 @foo() {
  %res = call i256 @llvm.eravm.factorydependency(metadata !1)
  ret i256 %res
}

define i256 @bar() {
  %res = call i256 @llvm.eravm.factorydependency(metadata !2)
  ret i256 %res
}

!1 = !{!"library_id"}
!2 = !{!"library_id"}
