; RUN: llc -O0 < %s | FileCheck %s

; ModuleID = 'Test_65'
source_filename = "Test_65"
target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: test_length 
define i256 @test_length(i256 %arg1) {
entry:

; CKECK: sub!    r2, r1, r2
; CKECK: add     0, r0, r1
; CKECK: add.eq  r3, r0, r2
  %comparison_result24 = icmp slt i256 35, %arg1
  %comparison_result_extended25 = zext i1 %comparison_result24 to i256

; CHECK: sub!    r1, r0, r1
; CHECK: add     0, r0, r1
; CHECK: add.ne  1, r0, r1
; CHECK: and     1, r1, r1
  %comparison_result26 = icmp eq i256 %comparison_result_extended25, 0
  %comparison_result_extended27 = zext i1 %comparison_result26 to i256

; CHECK: sub!    r1, r0, r1
; CHECK: add     0, r0, r1
; CHECK: add.eq  1, r0, r1
; CHECK: and     1, r1, r1
  %if_condition_compared28 = icmp ne i256 %comparison_result_extended27, 0
  %ret_value = zext i1 %if_condition_compared28 to i256
  ret i256 %ret_value
}

