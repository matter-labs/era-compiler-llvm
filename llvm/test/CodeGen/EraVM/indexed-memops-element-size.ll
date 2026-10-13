; RUN: llc -O3 --stop-after=eravm-indexed-memops-prepare --compile-twice=false < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; Check that the pointer is increased by one cell if the element is a pointer.
define i256 @test_ptr(ptr addrspace(1) %arg, i256 %n) {
; CHECK-LABEL: @test_ptr(
; CHECK: getelementptr inbounds ptr addrspace(1), ptr addrspace(1) %{{.*}}, i256 1
entry:
  br label %loop

loop:
  %phi = phi i256 [ 0, %entry ], [ %add, %loop ]
  %acc = phi i256 [ 0, %entry ], [ %acc.next, %loop ]
  %gep = getelementptr inbounds ptr addrspace(1), ptr addrspace(1) %arg, i256 %phi
  %load = load ptr addrspace(1), ptr addrspace(1) %gep, align 32
  %int = ptrtoint ptr addrspace(1) %load to i256
  %acc.next = add i256 %acc, %int
  %add = add nuw i256 %phi, 1
  %icmp = icmp eq i256 %add, %n
  br i1 %icmp, label %exit, label %loop

exit:
  ret i256 %acc.next
}

; Check that the pointer is increased by one cell if the element is i1, which
; occupies a byte.
define void @test_i1(ptr addrspace(1) %arg, i256 %n) {
; CHECK-LABEL: @test_i1(
; CHECK: getelementptr inbounds i1, ptr addrspace(1) %{{.*}}, i256 32
entry:
  br label %loop

loop:
  %phi = phi i256 [ 0, %entry ], [ %add, %loop ]
  %gep = getelementptr inbounds i1, ptr addrspace(1) %arg, i256 %phi
  store i256 1, ptr addrspace(1) %gep, align 1
  %add = add nuw i256 %phi, 32
  %icmp = icmp eq i256 %add, %n
  br i1 %icmp, label %exit, label %loop

exit:
  ret void
}
