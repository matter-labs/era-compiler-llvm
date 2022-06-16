; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: slt0
define i256 @slt0(i8 %par) nounwind {
  %1 = icmp slt i8 %par, 0
  %2 = zext i1 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: slt1
define i256 @slt1(i8 %par) nounwind {
  %1 = icmp slt i8 %par, 1
  %2 = zext i1 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: small_imm1
define i256 @small_imm1(i256 %par) nounwind {
  ; CHECK: sub.s!  65535, r{{[0-9]}}, r{{[0-9]}}
  %1 = icmp ugt i256 %par, 65535
  ; CHECK: add.gt 1, r{{[0-9]}}, r{{[0-9]}}
  %2 = zext i1 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: small_imm2
define i256 @small_imm2(i256 %par) nounwind {
  ; CHECK: sub.s!  65535, r{{[0-9]}}, r{{[0-9]}}
  %1 = icmp ugt i256 65535, %par
  ; CHECK: add.lt 1, r{{[0-9]}}, r{{[0-9]}}
  %2 = zext i1 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: large_imm1
define i256 @large_imm1(i256 %par) nounwind {
  ; CHECK: sub.s!  @CPI{{[0-9]+_[0-9]+}}[0], r{{[0-9]}}, r{{[0-9]}}
  %1 = icmp ugt i256 %par, 1234567890123456789
  ; CHECK: add.gt 1, r{{[0-9]}}, r{{[0-9]}}
  %2 = zext i1 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: large_imm2
define i256 @large_imm2(i256 %par) nounwind {
  ; CHECK: sub.s!  @CPI{{[0-9]+_[0-9]+}}[0], r{{[0-9]}}, r{{[0-9]}}
  %1 = icmp ugt i256 1234567890123456789, %par
  ; CHECK: add.lt 1, r{{[0-9]}}, r{{[0-9]}}
  %2 = zext i1 %1 to i256
  ret i256 %2
}
