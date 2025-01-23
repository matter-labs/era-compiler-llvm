; RUN: llc -O2 -filetype=obj --mtriple=evm %s -o - | llvm-objdump -r --headers - | FileCheck %s

; CHECK: .symbol_name__linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__ 0000000b

; CHECK: RELOCATION RECORDS FOR [.text]:
; CHECK-NEXT: OFFSET   TYPE        VALUE
; CHECK-NEXT: {{d*}} R_EVM_DATA __linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__0
; CHECK-NEXT: {{d*}} R_EVM_DATA __linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__1
; CHECK-NEXT: {{d*}} R_EVM_DATA __linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__2
; CHECK-NEXT: {{d*}} R_EVM_DATA __linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__3
; CHECK-NEXT: {{d*}} R_EVM_DATA __linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__4
; CHECK-NEXT: {{d*}} R_EVM_DATA __dataoffset__$0336fd807c0716a535e520df5b63ecc41ba7984875fdfa2241fcf3c8d0107e26$__
; CHECK-NEXT: {{d*}} R_EVM_DATA __datasize__$0336fd807c0716a535e520df5b63ecc41ba7984875fdfa2241fcf3c8d0107e26$__
; CHECK-NEXT: {{d*}} R_EVM_DATA __linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__0
; CHECK-NEXT: {{d*}} R_EVM_DATA __linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__1
; CHECK-NEXT: {{d*}} R_EVM_DATA __linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__2
; CHECK-NEXT: {{d*}} R_EVM_DATA __linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__3
; CHECK-NEXT: {{d*}} R_EVM_DATA __linker_symbol__$65c539c969b08d62684b6c973d3f0f4f37a85c2d0cbe39fffa3022cb69aac23a$__4
; CHECK-NEXT: {{d*}} R_EVM_DATA __dataoffset__$0336fd807c0716a535e520df5b63ecc41ba7984875fdfa2241fcf3c8d0107e26$__
; CHECK-NEXT: {{d*}} R_EVM_DATA __datasize__$0336fd807c0716a535e520df5b63ecc41ba7984875fdfa2241fcf3c8d0107e26$__

; TODO: CRP-1575. Rewrite the test in assembly.
target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

; Function Attrs: nounwind
declare i256 @llvm.evm.datasize(metadata)
declare i256 @llvm.evm.dataoffset(metadata)
declare i256 @llvm.evm.linkersymbol(metadata)

define i256 @foo() {
entry:
  %deployed_size = tail call i256 @llvm.evm.datasize(metadata !1)
  %deployed_off = tail call i256 @llvm.evm.dataoffset(metadata !1)
  %lib_addr = call i256 @llvm.evm.linkersymbol(metadata !2)
  %tmp = sub i256 %deployed_size, %deployed_off
  %res = sub i256 %tmp, %lib_addr
  ret i256 %res
}

define i256 @bar() {
entry:
  %deployed_size = tail call i256 @llvm.evm.datasize(metadata !1)
  %deployed_off = tail call i256 @llvm.evm.dataoffset(metadata !1)
  %lib_addr = call i256 @llvm.evm.linkersymbol(metadata !2)
  %tmp = sub i256 %deployed_size, %deployed_off
  %res = sub i256 %tmp, %lib_addr
  ret i256 %res
}

!1 = !{!"D_105_deployed"}
!2 = !{!"library_id2"}
