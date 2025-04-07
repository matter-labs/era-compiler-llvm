; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i256 @icmp_eq(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: icmp_eq:
; CHECK : ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : EQ [[RES:\$[0-9]+]], [[IN1]], [[IN2]]

  %cmp = icmp eq i256 %a, %b
  %res = zext i1 %cmp to i256
  ret i256 %res
}

define i256 @icmp_big_imm_eq(i256 %a) nounwind {
; CHECK-LABEL: icmp_big_imm_eq:
; CHECK : CONST_I256 [[C1:\$[0-9]+]], [[[0-9]+]]
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : EQ [[RES:\$[0-9]+]], [[IN1]], [[IN2]]

  %cmp = icmp eq i256 %a, 43576122634770472758325941782982599838796957244005075818703754470792663924736
  %res = zext i1 %cmp to i256
  ret i256 %res
}

define i256 @icmp_ne(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: icmp_ne:
; CHECK : ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : EQ [[TMP1:\$[0-9]+]], [[IN1]], [[IN2]]
; CHECK : ISZERO [[RES:\$[0-9]+]], [[TMP1]]

  %cmp = icmp ne i256 %a, %b
  %res = zext i1 %cmp to i256
  ret i256 %res
}

define i256 @icmp_ugt(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: icmp_ugt:
; CHECK : ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : GT [[RES:\$[0-9]+]], [[IN1]], [[IN2]]

  %cmp = icmp ugt i256 %a, %b
  %res = zext i1 %cmp to i256
  ret i256 %res
}

define i256 @icmp_uge(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: icmp_uge:
; CHECK : ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : LT [[TMP1:\$[0-9]+]], [[IN1]], [[IN2]]
; CHECK : ISZERO [[RES:\$[0-9]+]], [[TMP1]]

  %cmp = icmp uge i256 %a, %b
  %res = zext i1 %cmp to i256
  ret i256 %res
}

define i256 @icmp_ult(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: icmp_ult:
; CHECK : ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : LT [[RES:\$[0-9]+]], [[IN1]], [[IN2]]

  %cmp = icmp ult i256 %a, %b
  %res = zext i1 %cmp to i256
  ret i256 %res
}

define i256 @icmp_ule(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: icmp_ule:
; CHECK : ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : GT [[TMP1:\$[0-9]+]], [[IN1]], [[IN2]]
; CHECK : ISZERO [[RES:\$[0-9]+]], [[TMP1]]

  %cmp = icmp ule i256 %a, %b
  %res = zext i1 %cmp to i256
  ret i256 %res
}

define i256 @icmp_sgt(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: icmp_sgt:
; CHECK : ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : SGT [[RES:\$[0-9]+]], [[IN1]], [[IN2]]

  %cmp = icmp sgt i256 %a, %b
  %res = zext i1 %cmp to i256
  ret i256 %res
}

define i256 @icmp_sge(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: icmp_sge:
; CHECK : ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : SLT [[TMP1:\$[0-9]+]], [[IN1]], [[IN2]]
; CHECK : ISZERO [[RES:\$[0-9]+]], [[TMP1]]

  %cmp = icmp sge i256 %a, %b
  %res = zext i1 %cmp to i256
  ret i256 %res
}

define i256 @icmp_slt(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: icmp_slt:
; CHECK : ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : SLT [[RES:\$[0-9]+]], [[IN1]], [[IN2]]

  %cmp = icmp slt i256 %a, %b
  %res = zext i1 %cmp to i256
  ret i256 %res
}

define i256 @icmp_sle(i256 %a, i256 %b) nounwind {
; CHECK-LABEL: icmp_sle:
; CHECK : ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK : ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK : SGT [[TMP1:\$[0-9]+]], [[IN1]], [[IN2]]
; CHECK : ISZERO [[RES:\$[0-9]+]], [[TMP1]]

  %cmp = icmp sle i256 %a, %b
  %res = zext i1 %cmp to i256
  ret i256 %res
}
