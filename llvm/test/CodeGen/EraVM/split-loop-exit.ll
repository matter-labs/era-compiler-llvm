; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test(ptr addrspace(1) %arg, ptr addrspace(1) %arg1, i256 %arg2) {
; CHECK-LABEL: test
; CHECK:         add @CPI0_0[0], r0, r3
; CHECK-NEXT:  .BB0_4:
; CHECK-NEXT:    add r3, r0, r1
; CHECK-NEXT:    ret

entry:
  %cmp = icmp eq i256 %arg2, 0
  br i1 %cmp, label %body, label %return

body:
  %phi = phi i256 [ 0, %entry ], [ %add, %body ]
  %gep = getelementptr inbounds i256, ptr addrspace(1) %arg1, i256 %phi
  %load = load i256, ptr addrspace(1) %gep, align 4
  %gep1 = getelementptr inbounds i256, ptr addrspace(1) %arg, i256 %phi
  store i256 %load, ptr addrspace(1) %gep1, align 4
  %add = add i256 %phi, 1
  %cmp1 = icmp ult i256 %add, 64
  br i1 %cmp1, label %body, label %return

return:
  %ret = phi i256 [ 0, %entry ], [ 10942859180823018302938210, %body ]
  ret i256 %ret
}
