; RUN: llc -opaque-pointers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42
declare void @foo(i256 %val)

; CHECK-LABEL: ADDrrr_v:
define i1 @ADDrrr_v(i256 %p1, i256 %p2) nounwind {
; CHECK: add! r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = add i256 %p1, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: ANDrrr_v:
define i1 @ANDrrr_v(i256 %p1, i256 %p2) nounwind {
; CHECK: and! r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = and i256 %p1, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: XORrrr_v:
define i1 @XORrrr_v(i256 %p1, i256 %p2) nounwind {
; CHECK: xor! r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = xor i256 %p1, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: ORrrr_v:
define i1 @ORrrr_v(i256 %p1, i256 %p2) nounwind {
; CHECK: or! r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = or i256 %p1, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHLrrr_v:
define i1 @SHLrrr_v(i256 %p1, i256 %p2) nounwind {
; CHECK: shl! r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = shl i256 %p1, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHRrrr_v:
define i1 @SHRrrr_v(i256 %p1, i256 %p2) nounwind {
; CHECK: shr! r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = lshr i256 %p1, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: MULrrrr_v:
define i1 @MULrrrr_v(i256 %p1, i256 %p2) nounwind {
; CHECK: mul! r1, r2, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = mul i256 %p1, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: DIVrrrr_v:
define i1 @DIVrrrr_v(i256 %p1, i256 %p2) nounwind {
; CHECK: div! r1, r2, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = udiv i256 %p1, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: ANDirr_v:
define i1 @ANDirr_v(i256 %p1) nounwind {
; CHECK: and! 123, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = and i256 %p1, 123
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: XORirr_v:
define i1 @XORirr_v(i256 %p1) nounwind {
; DAG combiner canonicalize xor to sub
; CHECK: sub.s! 123, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = xor i256 %p1, 123
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: XORirr_Select:
define i256 @XORirr_Select(i256 %p1) nounwind {
; CHECK: xor! 123, r{{[0-9]+}}, r1
; CHECK: add.eq 42, r0, r1
  %p3 = xor i256 %p1, 123
  %cmp = icmp eq i256 %p3, 0
  %res = select i1 %cmp, i256 42, i256 %p3
  ret i256 %res
}

; CHECK-LABEL: ORirr_v:
define i1 @ORirr_v(i256 %p1) nounwind {
; Can't be equal to 0, optimized away.
; CHECK: add r0, r0, r1
  %p3 = or i256 %p1, 123
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHLirr_v:
define i1 @SHLirr_v(i256 %p2) nounwind {
; CHECK: shl! 42, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = shl i256 42, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHRirr_v:
define i1 @SHRirr_v(i256 %p2) nounwind {
; CHECK: shr! 42, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = lshr i256 42, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHLxrr_v:
define i1 @SHLxrr_v(i256 %p1) nounwind {
; CHECK: shl.s! 42, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = shl i256 %p1, 42
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHRxrr_v:
define i1 @SHRxrr_v(i256 %p1) nounwind {
; CHECK: shr.s! 42, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = lshr i256 %p1, 42
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: MULirrr_v:
define i1 @MULirrr_v(i256 %p1) nounwind {
; CHECK: mul! 123, r1, r{{[0-9]+}}
  %p3 = mul i256 %p1, 123
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: DIVirrr_v:
define i1 @DIVirrr_v(i256 %p2) nounwind {
; CHECK: div! 123, r1, r{{[0-9]+}}
  %p3 = udiv i256 123, %p2
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: DIVxrrr_v:
define i1 @DIVxrrr_v(i256 %p1) nounwind {
; CHECK: div.s! 123, r1, r{{[0-9]+}}
  %p3 = udiv i256 %p1, 123
  %cmp = icmp eq i256 %p3, 0
  ret i1 %cmp
}

; CHECK-LABEL: ADDcrr_v:
define i1 @ADDcrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: add! @val[0], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = add i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: ANDcrr_v:
define i1 @ANDcrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: and! @val[0], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = and i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: ORcrr_v:
define i1 @ORcrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: or! @val[0], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = or i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: XORcrr_v:
define i1 @XORcrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: xor! @val[0], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = xor i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHLcrr_v:
define i1 @SHLcrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: shl! @val[0], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = shl i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHRcrr_v:
define i1 @SHRcrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: shr! @val[0], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = lshr i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHLyrr_v:
define i1 @SHLyrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: shl.s! @val[0], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = shl i256 %p1, %val
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHRyrr_v:
define i1 @SHRyrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: shr.s! @val[0], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = lshr i256 %p1, %val
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: MULcrrr_v:
define i1 @MULcrrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: mul! @val[0], r1, r{{[0-9]+}}
  %p2 = mul i256 %p1, %val
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: DIVcrrr_v:
define i1 @DIVcrrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: div! @val[0], r1, r{{[0-9]+}}
  %p2 = udiv i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: DIVyrrr_v:
define i1 @DIVyrrr_v(i256 %p1) nounwind {
  %val = load i256, i256 addrspace(4)* @val
; CHECK: div.s! @val[0], r1, r{{[0-9]+}}
  %p2 = udiv i256 %p1, %val
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: ADDsrr_v:
define i1 @ADDsrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: add! stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = add i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: ANDsrr_v:
define i1 @ANDsrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: and! stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = and i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: ORsrr_v:
define i1 @ORsrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: or! stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 =  or i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: XORsrr_v:
define i1 @XORsrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: xor! stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 =  xor i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHLsrr_v:
define i1 @SHLsrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: shl! stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = shl i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHRsrr_v:
define i1 @SHRsrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: shr! stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = lshr i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHLzrr_v:
define i1 @SHLzrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: shl.s! stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = shl i256 %p1, %val
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: SHRzrr_v:
define i1 @SHRzrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: shr.s! stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %p2 = lshr i256 %p1, %val
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: MULsrrr_v:
define i1 @MULsrrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: mul! stack-[1], r1, r{{[0-9]+}}
  %p2 = mul i256 %p1, %val
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: DIVsrrr_v:
define i1 @DIVsrrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: div! stack-[1], r1, r{{[0-9]+}}
  %p2 = udiv i256 %val, %p1
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: DIVzrrr_v:
define i1 @DIVzrrr_v(i256 %p1) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
; CHECK: div.s! stack-[1], r1, r{{[0-9]+}}
  %p2 = udiv i256 %p1, %val
  %cmp = icmp eq i256 %p2, 0
  ret i1 %cmp
}

; CHECK-LABEL: NoCombine:
define i1 @NoCombine(i256 %p1, i1 %sel, i256 %random) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
  %p2 =  xor i256 %val, %p1
; We cannot combine xor with icmp because `select` will overwrite flags
; CHECK: xor stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %s = select i1 %sel, i256 %p1, i256 %p2
  %cmp = icmp eq i256 %s, 0
  ret i1 %cmp
}

; CHECK-LABEL: NoCombine2:
define i1 @NoCombine2(i256 %p1, i256 %c) nounwind {
  %valptr = alloca i256
  %val = load i256, i256* %valptr
  %p2 =  xor i256 %val, %p1
; Cannot combine xor with icmp because there are another `icmp` in between
; CHECK: xor stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %cmp = icmp ult i256 %p2, %c
  %cmp2 = icmp eq i256 %p2, 0
  %res = and i1 %cmp, %cmp2
  ret i1 %res
}
