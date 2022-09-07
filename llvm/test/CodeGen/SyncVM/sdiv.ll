; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: sremrrr
define i256 @sremrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK:        div.s   @CPI0_0[0], [[REG2:r[0-9]+]], [[REG2]], [[REG3:r[0-9]+]]
; CHECK-NEXT:   add     @CPI0_0[0], r0, [[REG4:r[0-9]+]]
; CHECK-NEXT:   sub     [[REG4]], [[REG3:r[0-9]+]], [[REG5:r[0-9]+]]
; CHECK-NEXT:   sub.s!  0, [[REG2]], [[REG2]]
; CHECK-NEXT:   add     [[REG3]], r0, [[REG2]]
; CHECK-NEXT:   add.ne  [[REG5]], r0, [[REG2]]
; CHECK-NEXT:   div.s   @CPI0_0[0], [[REG1:r[0-9]+]], [[REG3]], [[REG5]]
; CHECK-NEXT:   sub     [[REG4]], [[REG5]], [[REG4]]
; CHECK-NEXT:   sub.s!  0, [[REG3]], [[REG3]]
; CHECK-NEXT:   add     [[REG5]], r0, [[REG3]]
; CHECK-NEXT:   add.ne  [[REG4]], r0, [[REG3]]
; CHECK-NEXT:   div     [[REG3]], [[REG2]], [[REG2]], [[REG3]]
; CHECK-NEXT:   and     @CPI0_0[0], [[REG1]], [[REG1]]
; CHECK-NEXT:   sub     0, [[REG3]], [[REG2]]
; CHECK-NEXT:   sub.s!  0, [[REG1]], [[REG1]]
; CHECK-NEXT:   add     [[REG2]], r0, [[REG1]]
; CHECK-NEXT:   add.eq  [[REG3]], r0, [[REG1]]
; CHECK-NEXT:   sub.s!  0, [[REG3]], [[REG2]]
; CHECK-NEXT:   add.eq  [[REG3]], r0, [[REG1]]
  %res = srem i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: sremirr
define i256 @sremirr(i256 %rs1) nounwind {
; TODO: Special handling of signed division by a constant
  %res = urem i256 42, %rs1
  ret i256 %res
}

; CHECK-LABEL: sremxrr
define i256 @sremxrr(i256 %rs1) nounwind {
; TODO: Special handling of signed division by a constant
  %res = srem i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: sremcrr
define i256 @sremcrr(i256 %rs1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
  %res = srem i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: sremyrr
define i256 @sremyrr(i256 %rs1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
  %res = srem i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: sremsrr
define i256 @sremsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
  %res = srem i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: sremzrr
define i256 @sremzrr(i256 %rs1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
  %res = srem i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: sremrrs
define void @sremrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = srem i256 %rs1, %rs2
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: sremirs
define void @sremirs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %res = srem i256 42, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: sremxrs
define void @sremxrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %res = srem i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: sremcrs
define void @sremcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256 addrspace(4)* @val
  %res = srem i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: sremyrs
define void @sremyrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256 addrspace(4)* @val
  %res = srem i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: sremsrs
define void @sremsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
  %val = load i256, i256* %valptr
  %res = srem i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: sremzrs
define void @sremzrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
  %val = load i256, i256* %valptr
  %res = srem i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: sdivrrr
define i256 @sdivrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK:         div.s   @CPI14_0[0], [[REG2]], [[REG2]], [[REG3]]
; CHECK-NEXT:    add     @CPI14_0[0], r0, [[REG4]]
; CHECK-NEXT:    sub     [[REG4]], [[REG3]], [[REG5]]
; CHECK-NEXT:    sub.s!  0, [[REG2]], r6
; CHECK-NEXT:    add.ne  [[REG5]], r0, [[REG3]]
; CHECK-NEXT:    div.s   @CPI14_0[0], [[REG1]], [[REG1]], [[REG5]]
; CHECK-NEXT:    xor     [[REG1]], [[REG2]], [[REG2]]
; CHECK-NEXT:    sub     [[REG4]], [[REG5]], [[REG4]]
; CHECK-NEXT:    sub.s!  0, [[REG1]], [[REG1]]
; CHECK-NEXT:    add     [[REG5]], r0, [[REG1]]
; CHECK-NEXT:    add.ne  [[REG4]], r0, [[REG1]]
; CHECK-NEXT:    div     [[REG1]], [[REG3]], [[REG1]], [[REG3]]
; CHECK-NEXT:    shl.s   255, [[REG2]], [[REG2]]
; CHECK-NEXT:    sub     [[REG2]], [[REG1]], [[REG3]]
; CHECK-NEXT:    or      [[REG3]], [[REG2]], [[REG3]]
; CHECK-NEXT:    sub.s!  0, [[REG2]], [[REG2]]
; CHECK-NEXT:    add     [[REG3]], r0, [[REG2]]
; CHECK-NEXT:    add.eq  [[REG1]], r0, [[REG2]]
; CHECK-NEXT:    sub.s!  0, [[REG1]], [[REG3]]
; CHECK-NEXT:    add.ne  [[REG2]], r0, [[REG1]]
  %res = sdiv i256 %rs1, %rs2
  ret i256 %res
}

; CHECK-LABEL: sdivirr
define i256 @sdivirr(i256 %rs1) nounwind {
; TODO: div 42, [[REG1]], [[REG1]], r{{[0-9]+}}
  %res = sdiv i256 42, %rs1
  ret i256 %res
}

; CHECK-LABEL: sdivxrr
define i256 @sdivxrr(i256 %rs1) nounwind {
; TODO: div.s 42, [[REG1]], [[REG1]], r{{[0-9]+}}
  %res = sdiv i256 %rs1, 42
  ret i256 %res
}

; CHECK-LABEL: sdivcrr
define i256 @sdivcrr(i256 %rs1) nounwind {
; TODO: div @val[0], [[REG1]], [[REG1]], r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = sdiv i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: sdivyrr
define i256 @sdivyrr(i256 %rs1) nounwind {
; TODO: div.s @val[0], [[REG1]], [[REG1]], r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res = sdiv i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: sdivsrr
define i256 @sdivsrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: div stack-[1], [[REG1]], [[REG1]], r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = sdiv i256 %val, %rs1
  ret i256 %res
}

; CHECK-LABEL: sdivzrr
define i256 @sdivzrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; TODO: div.s stack-[1], [[REG1]], [[REG1]], r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = sdiv i256 %rs1, %val
  ret i256 %res
}

; CHECK-LABEL: sdivrrs
define void @sdivrrs(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = sdiv i256 %rs1, %rs2
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: sdivirs
define void @sdivirs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %res = sdiv i256 42, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: sdivxrs
define void @sdivxrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %res = sdiv i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: sdivcrs
define void @sdivcrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256 addrspace(4)* @val
  %res = sdiv i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: sdivyrs
define void @sdivyrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256 addrspace(4)* @val
  %res = sdiv i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret void
}

; CHECK-LABEL: sdivsrs
define void @sdivsrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
  %val = load i256, i256* %valptr
  %res = sdiv i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: sdivzrs
define void @sdivzrs(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
; TODO: div.s stack-[2], [[REG1]], stack-[1], r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res = sdiv i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret void
}

; CHECK-LABEL: sdivremrrrr
define i256 @sdivremrrrr(i256 %rs1, i256 %rs2) nounwind {
; CHECK:       div.s   @CPI28_0[0], [[REG2]], [[REG2]], [[REG3]]
; CHECK-NEXT:  add     @CPI28_0[0], r0, [[REG4]]
; CHECK-NEXT:  sub     [[REG4]], [[REG3]], [[REG5]]
; CHECK-NEXT:  sub.s!  0, [[REG2]], [[REG6:r[0-9]+]]
; CHECK-NEXT:  add.ne  [[REG5]], r0, [[REG3]]
; CHECK-NEXT:  div.s   @CPI28_0[0], [[REG1]], [[REG5]], [[REG6]]
; CHECK-NEXT:  xor     [[REG5]], [[REG2]], [[REG2]]
; CHECK-NEXT:  sub     [[REG4]], [[REG6]], [[REG4]]
; CHECK-NEXT:  sub.s!  0, [[REG5]], [[REG5]]
; CHECK-NEXT:  add.eq  [[REG6]], r0, [[REG4]]
; CHECK-NEXT:  div     [[REG4]], [[REG3]], [[REG3]], [[REG4]]
; CHECK-NEXT:  shl.s   255, [[REG2]], [[REG2]]
; CHECK-NEXT:  sub     [[REG2]], [[REG3]], [[REG5]]
; CHECK-NEXT:  or      [[REG5]], [[REG2]], [[REG5]]
; CHECK-NEXT:  sub.s!  0, [[REG2]], [[REG2]]
; CHECK-NEXT:  add     [[REG5]], r0, [[REG2]]
; CHECK-NEXT:  add.eq  [[REG3]], r0, [[REG2]]
; CHECK-NEXT:  and     @CPI28_0[0], [[REG1]], [[REG1]]
; CHECK-NEXT:  sub     0, [[REG4]], [[REG5]]
; CHECK-NEXT:  sub.s!  0, [[REG1]], [[REG1]]
; CHECK-NEXT:  add     [[REG5]], r0, [[REG1]]
; CHECK-NEXT:  add.eq  [[REG4]], r0, [[REG1]]
; CHECK-NEXT:  sub.s!  0, [[REG4]], [[REG5]]
; CHECK-NEXT:  add.eq  [[REG4]], r0, [[REG1]]
; CHECK-NEXT:  sub.s!  0, [[REG3]], [[REG4]]
; CHECK-NEXT:  add.eq  [[REG3]], r0, [[REG2]]
  %res1 = srem i256 %rs1, %rs2
  %res2 = sdiv i256 %rs1, %rs2
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: sdivremirrr
define i256 @sdivremirrr(i256 %rs1) nounwind {
; CHECK: div 42, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %res1 = srem i256 42, %rs1
  %res2 = sdiv i256 42, %rs1
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: sdivremxrrr
define i256 @sdivremxrrr(i256 %rs1) nounwind {
; CHECK: div r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %res1 = srem i256 %rs1, 42
  %res2 = sdiv i256 %rs1, 42
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: sdivremcrrr
define i256 @sdivremcrrr(i256 %rs1) nounwind {
; CHECK: div r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res1 = srem i256 %val, %rs1
  %res2 = sdiv i256 %val, %rs1
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: sdivremyrrr
define i256 @sdivremyrrr(i256 %rs1) nounwind {
; CHECK: div r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %val = load i256, i256 addrspace(4)* @val
  %res1 = srem i256 %rs1, %val
  %res2 = sdiv i256 %rs1, %val
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: sdivremsrrr
define i256 @sdivremsrrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res1 = srem i256 %val, %rs1
  %res2 = sdiv i256 %val, %rs1
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: sdivremzrrr
define i256 @sdivremzrrr(i256 %rs1) nounwind {
  %valptr = alloca i256
; CHECK: div r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %val = load i256, i256* %valptr
  %res1 = srem i256 %rs1, %val
  %res2 = sdiv i256 %rs1, %val
  %res = add i256 %res1, %res2
  ret i256 %res
}

; CHECK-LABEL: sdivremrrsr
define i256 @sdivremrrsr(i256 %rs1, i256 %rs2) nounwind {
  %valptr = alloca i256
  %res = sdiv i256 %rs1, %rs2
  %rem = srem i256 %rs1, %rs2
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: sdivremirsr
define i256 @sdivremirsr(i256 %rs1) nounwind {
  %valptr = alloca i256
  %res = sdiv i256 42, %rs1
  %rem = srem i256 42, %rs1
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: sdivremxrsr
define i256 @sdivremxrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
  %res = sdiv i256 %rs1, 42
  %rem = srem i256 %rs1, 42
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: sdivremcrsr
define i256 @sdivremcrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256 addrspace(4)* @val
  %res = sdiv i256 %val, %rs1
  %rem = srem i256 %val, %rs1
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: sdivremyrsr
define i256 @sdivremyrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256 addrspace(4)* @val
  %res = sdiv i256 %rs1, %val
  %rem = srem i256 %rs1, %val
  store i256 %res, i256* %valptr
  ret i256 %rem
}

; CHECK-LABEL: sdivremsrsr
define i256 @sdivremsrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
  %val = load i256, i256* %valptr
  %res = sdiv i256 %val, %rs1
  %rem = srem i256 %val, %rs1
  store i256 %res, i256* %destptr
  ret i256 %rem
}

; CHECK-LABEL: sdivremzrsr
define i256 @sdivremzrsr(i256 %rs1) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256
  %val = load i256, i256* %valptr
  %res = sdiv i256 %rs1, %val
  %rem = srem i256 %rs1, %val
  store i256 %res, i256* %destptr
  ret i256 %rem
}
