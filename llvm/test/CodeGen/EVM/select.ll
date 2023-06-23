; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @select(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
; CHECK-LABEL: @select
; CHECK: ARGUMENT [[IN4:\$[0-9]+]], 3
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: EQ [[TMP1:\$[0-9]+]], [[IN3]], [[IN4]]
; CHECK: ISZERO [[COND:\$[0-9]+]], [[TMP1]]
; CHECK: JUMPI @.BB0_2, [[COND]]
; CHECK: COPY_I256 [[IN1]], [[IN2]]
; CHECK-LABEL: .BB0_2:
; CHECK: RET

  %1 = icmp ne i256 %v3, %v4
  %2 = select i1 %1, i256 %v1, i256 %v2
  ret i256 %2
}
