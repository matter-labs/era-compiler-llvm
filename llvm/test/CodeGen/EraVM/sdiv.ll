; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "eravm"

; CHECK-LABEL: srem
define i256 @srem(i256 %rs1, i256 %rs2) nounwind {
; CHECK:        div.s!  @CPI0_0[0], [[REG2:r[0-9]+]], [[REG2]], [[REG3:r[0-9]+]]
; CHECK-NEXT:   sub     @CPI0_0[0], [[REG3:r[0-9]+]], [[REG4:r[0-9]+]]
; CHECK-NEXT:   add.eq  [[REG3]], r0, [[REG4]]
; CHECK-NEXT:   div.s!  @CPI0_0[0], [[REG1:r[0-9]+]], [[REG3]], [[REG5:r[0-9]+]]
; CHECK-NEXT:   sub     @CPI0_0[0], [[REG5]], [[REG2]]
; CHECK-NEXT:   add.eq  [[REG5]], r0, [[REG2]]
; CHECK-NEXT:   div     [[REG2]], [[REG4]], [[REG3]], [[REG2]]
; CHECK-NEXT:   and!    @CPI0_0[0], [[REG1]], [[REG1]]
; CHECK-NEXT:   sub     0, [[REG2]], [[REG1]]
; CHECK-NEXT:   add.eq  [[REG2]], r0, [[REG1]]
; CHECK-NEXT:   sub!  [[REG2]], r0, [[REG3]]
; CHECK-NEXT:   add.ne  [[REG1]], r0, [[REG2]]
  %res = srem i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: sdiv
define i256 @sdiv(i256 %rs1, i256 %rs2) nounwind {
; CHECK:             div.s!  @CPI{{[0-9]+}}_0[0], [[REGsdiv2:r[0-9]+]], [[REGsdiv2]], [[REGsdiv3:r[0-9]+]]
; CHECK-NEXT:        sub     @CPI{{[0-9]+}}_0[0], [[REGsdiv3]], [[REGsdiv5:r[0-9]+]]
; CHECK-NEXT:        add.eq  [[REGsdiv3]], r0, [[REGsdiv5]]
; CHECK-NEXT:        div.s!  @CPI{{[0-9]+}}_0[0], [[REGsdiv1:r[0-9]+]], [[REGsdiv1]], [[REGsdiv3]]
; CHECK-NEXT:        xor     [[REGsdiv1]], [[REGsdiv2]], [[REGsdiv2]]
; CHECK-NEXT:        sub     @CPI{{[0-9]+}}_0[0], [[REGsdiv3]], [[REGsdiv1]]
; CHECK-NEXT:        add.eq  [[REGsdiv3]], r0, [[REGsdiv1]]
; CHECK-NEXT:        div     [[REGsdiv1]], [[REGsdiv5]], [[REGsdiv1]], [[REGsdiv3]]
; CHECK-NEXT:        shl.s!  255, [[REGsdiv2]], [[REGsdiv2]]
; CHECK-NEXT:        sub     [[REGsdiv2]], [[REGsdiv1]], [[REGsdiv3]]
; CHECK-NEXT:        or      [[REGsdiv3]], [[REGsdiv2]], [[REGsdiv2]]
; CHECK-NEXT:        add.eq  [[REGsdiv1]], r0, [[REGsdiv2]]
; CHECK-NEXT:        sub!    [[REGsdiv1]], r0, [[REGsdiv3]]
; CHECK-NEXT:        add.ne  [[REGsdiv2]], r0, [[REGsdiv1]]
  %res = sdiv i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: sdivrem
define i256 @sdivrem(i256 %rs1, i256 %rs2) nounwind {
; CHECK:       div.s!  @CPI{{[0-9]+}}_0[0], [[REGsdivrem2:r[0-9]+]], [[REGsdivrem2]], [[REGsdivrem3:r[0-9]+]]
; CHECK-NEXT:  sub     @CPI{{[0-9]+}}_0[0], [[REGsdivrem3]], [[REGsdivrem5:r[0-9]+]]
; CHECK-NEXT:  add.eq  [[REGsdivrem3]], r0, [[REGsdivrem5]]
; CHECK-NEXT:  div.s!  @CPI{{[0-9]+}}_0[0], [[REGsdivrem1:r[0-9]+]], [[REGsdivrem3]], [[REGsdivrem6:r[0-9]+]]
; CHECK-NEXT:  xor     [[REGsdivrem3]], [[REGsdivrem2]], [[REGsdivrem2]]
; CHECK-NEXT:  sub     @CPI{{[0-9]+}}_0[0], [[REGsdivrem6]], [[REGsdivrem3]]
; CHECK-NEXT:  add.eq  [[REGsdivrem6]], r0, [[REGsdivrem3]]
; CHECK-NEXT:  div     [[REGsdivrem3]], [[REGsdivrem5]], [[REGsdivrem4:r[0-9]+]], [[REGsdivrem3]]
; CHECK-NEXT:  shl.s!  255, [[REGsdivrem2]], [[REGsdivrem2]]
; CHECK-NEXT:  sub     [[REGsdivrem2]], [[REGsdivrem4]], [[REGsdivrem5]]
; CHECK-NEXT:  or      [[REGsdivrem5]], [[REGsdivrem2]], [[REGsdivrem2]]
; CHECK-NEXT:  add.eq  [[REGsdivrem4]], r0, [[REGsdivrem2]]
; CHECK-NEXT:  and!    @CPI{{[0-9]+}}_0[0], [[REGsdivrem1]], [[REGsdivrem1]]
; CHECK-NEXT:  sub     0, [[REGsdivrem3]], [[REGsdivrem1]]
; CHECK-NEXT:  add.eq  [[REGsdivrem3]], r0, [[REGsdivrem1]]
; CHECK-NEXT:  sub!    [[REGsdivrem3]], r0, [[REGsdivrem5]]
; CHECK-NEXT:  add.ne  [[REGsdivrem1]], r0, [[REGsdivrem3]]
; CHECK-NEXT:  sub!    [[REGsdivrem4]], r0, [[REGsdivrem1]]
; CHECK-NEXT:  add.ne  [[REGsdivrem2]], r0, [[REGsdivrem4]]
; CHECK-NEXT:  add     [[REGsdivrem3]], [[REGsdivrem4]], [[REGsdivrem1]]
  %res1 = srem i256 %rs1, %rs2
  %res2 = sdiv i256 %rs1, %rs2
  %res = add i256 %res1, %res2
  ret i256 %res
}
