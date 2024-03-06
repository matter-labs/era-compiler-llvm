; RUN: llc -O3 -stop-after=finalize-isel -compile-twice=false < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define ptr addrspace(3) @test(ptr addrspace(3) %addr) {
; CHECK-LABEL: test
; CHECK:         %0:grptr = fatptr COPY $r1
; CHECK-NEXT:    $r1 = COPY %0
; CHECK-NEXT:    RET i256 0, implicit $r1
  ret ptr addrspace(3) %addr
}
