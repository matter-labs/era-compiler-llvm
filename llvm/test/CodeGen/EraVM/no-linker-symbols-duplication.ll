; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare i256 @llvm.eravm.linkersymbol(metadata)

; CHECK: .linker_symbol0:
; CHECK-NOT: .linker_symbol1:

define i256 @foo() {
  %res = call i256 @llvm.eravm.linkersymbol(metadata !1)
  ret i256 %res
}

define i256 @bar() {
  %res = call i256 @llvm.eravm.linkersymbol(metadata !2)
  ret i256 %res
}

!1 = !{!"library_id"}
!2 = !{!"library_id"}
