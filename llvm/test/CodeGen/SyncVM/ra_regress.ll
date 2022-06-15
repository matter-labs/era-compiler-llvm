; RUN: llc -O0 < %s | FileCheck %s

; ModuleID = 'Test_65'
source_filename = "Test_65"
target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: test_length 
define i256 @test_length(i256 %arg1) {
entry:
; CHECK:      sub.s!  36, r{{[0-9]+}}, r{{[[0-9]+}}
; CHECK-NEXT: add     0, r{{[0-9]+}}, r{{[[0-9]+}}
; CHECK-NEXT: add.lt  r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %comparison_result24 = icmp slt i256 35, %arg1
  %comparison_result_extended25 = zext i1 %comparison_result24 to i256

; CHECK:      sub.s!  0, r{{[0-9]+}}, r{{[[0-9]+}}
; CHECK-NEXT: add.le  0, r{{[0-9]+}}, r{{[[0-9]+}}
  %comparison_result26 = icmp eq i256 %comparison_result_extended25, 0
  %comparison_result_extended27 = zext i1 %comparison_result26 to i256

; CHECK:      sub.s!  0, r{{[0-9]+}}, r{{[[0-9]+}}
; CHECK-NEXT: add     0, r{{[0-9]+}}, r{{[[0-9]+}}
; CHECK-NEXT: add.ne  1, r{{[0-9]+}}, r{{[[0-9]+}}
; CHECK-NEXT: and     1, r{{[0-9]+}}, r{{[[0-9]+}}
  %if_condition_compared28 = icmp ne i256 %comparison_result_extended27, 0
  %ret_value = zext i1 %if_condition_compared28 to i256
  ret i256 %ret_value
}

