; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @addmod(i256 %rs1, i256 %rs2, i256 %rs3) nounwind {
; CHECK-LABEL: @addmod
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: ADDMOD [[TMP:\$[0-9]+]], [[IN1]], [[IN2]], [[IN3]]

  %res = call i256 @llvm.evm.addmod(i256 %rs1, i256 %rs2, i256 %rs3)
  ret i256 %res
}

define i256 @mulmod(i256 %rs1, i256 %rs2, i256 %rs3) nounwind {
; CHECK-LABEL: @mulmod
; CHECK: ARGUMENT [[IN3:\$[0-9]+]], 2
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: MULMOD [[TMP:\$[0-9]+]], [[IN1]], [[IN2]], [[IN3]]

  %res = call i256 @llvm.evm.mulmod(i256 %rs1, i256 %rs2, i256 %rs3)
  ret i256 %res
}

define i256 @exp(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: @exp
; CHECK: ARGUMENT [[IN2:\$[0-9]+]], 1
; CHECK: ARGUMENT [[IN1:\$[0-9]+]], 0
; CHECK: EXP [[TMP:\$[0-9]+]], [[IN1]], [[IN2]]

  %res = call i256 @llvm.evm.exp(i256 %rs1, i256 %rs2)
  ret i256 %res
}

declare i256 @llvm.evm.addmod(i256, i256, i256)
declare i256 @llvm.evm.mulmod(i256, i256, i256)
declare i256 @llvm.evm.exp(i256, i256)
