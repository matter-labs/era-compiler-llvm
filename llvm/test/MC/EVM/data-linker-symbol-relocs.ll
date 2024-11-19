; RUN: llc -O2 -filetype=obj --mtriple=evm %s -o - | llvm-objdump -r - | FileCheck %s

; CHECK: RELOCATION RECORDS FOR [.text]:
; CHECK-NEXT: OFFSET   TYPE        VALUE
; CHECK-NEXT: {{\d*}} R_EVM_DATA   __dataoffset__$0336fd807c0716a535e520df5b63ecc41ba7984875fdfa2241fcf3c8d0107e26$__
; CHECK-NEXT: {{\d*}} R_EVM_DATA   __datasize__$0336fd807c0716a535e520df5b63ecc41ba7984875fdfa2241fcf3c8d0107e26$__
; CHECK-NEXT: {{\d*}} R_EVM_DATA   __dataoffset__$0336fd807c0716a535e520df5b63ecc41ba7984875fdfa2241fcf3c8d0107e26$__
; CHECK-NEXT: {{\d*}} R_EVM_DATA   __datasize__$0336fd807c0716a535e520df5b63ecc41ba7984875fdfa2241fcf3c8d0107e26$__

; TODO: CRP-1575. Rewrite the test in assembly.
target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

; Function Attrs: nounwind
declare i256 @llvm.evm.datasize(metadata)
declare i256 @llvm.evm.dataoffset(metadata)

define i256 @foo() {
entry:
  %deployed_size = tail call i256 @llvm.evm.datasize(metadata !1)
  %deployed_off = tail call i256 @llvm.evm.dataoffset(metadata !1)
  %res = add i256 %deployed_size, %deployed_off
  ret i256 %res
}

define i256 @bar() {
entry:
  %deployed_size = tail call i256 @llvm.evm.datasize(metadata !1)
  %deployed_off = tail call i256 @llvm.evm.dataoffset(metadata !1)
  %res = sub i256 %deployed_size, %deployed_off
  ret i256 %res
}

!1 = !{!"D_105_deployed"}
