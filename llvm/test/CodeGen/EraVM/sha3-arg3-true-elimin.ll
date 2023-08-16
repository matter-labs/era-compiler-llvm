; RUN: opt -O2 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare i256 @__sha3(i256 %0, i256 %1, i1 %throw_at_failure) #0

; CHECK-LABEL: @__sha3
; CHECK-NOT: failure_block:
; CHECK-NOT: revert_block:

define i256 @test_true(i256 %0, i256 %1) {
  %sha1 = call i256 @__sha3(i256 %0, i256 %1, i1 true)
  %sha2 = call i256 @__sha3(i256 %0, i256 %1, i1 true)
  %ret = add i256 %sha1, %sha2
  ret i256 %ret
}

attributes #0 = { nofree null_pointer_is_valid }
