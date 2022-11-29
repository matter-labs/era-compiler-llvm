; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42

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
; TODO: CPR-895 xor! 123, r{{[0-9]+}}, r1
  %p3 = xor i256 %p1, 123
  %cmp = icmp eq i256 %p3, 0
  %res = select i1 %cmp, i256 42, i256 %p3
  ret i256 %p3
}

; CHECK-LABEL: ORirr_v:
define i1 @ORirr_v(i256 %p1) nounwind {
; CHECK: or! 123, r{{[0-9]+}}, r{{[0-9]+}}
  %p3 = or i256 %p1, 123
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
