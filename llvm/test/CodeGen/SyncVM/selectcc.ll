; TODO: restore
; XFAIL: *
; RUN: llc < %s | FileCheck %s

target triple = "syncvm"
target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"

@val = addrspace(4) global i256 42
@val2 = addrspace(4) global i256 17

; CHECK-LABEL: selopt
define i256 @selopt(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ne i256 %v3, %v4
  ; CHECK: add.eq r2, r0, r1
  %2 = select i1 %1, i256 %v1, i256 %v2
  ret i256 %2
}

; CHECK-LABEL: selrrr
define i256 @selrrr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp eq i256 %v3, %v4
  ; CHECK: add.ne r2, r0, r1
  %2 = select i1 %1, i256 %v1, i256 %v2
  ret i256 %2
}

; CHECK-LABEL: selirr
define i256 @selirr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ugt i256 %v3, %v4
  ; CHECK: add r2, r0, r1
  ; CHECK: add.gt 42, r0, r1
  %2 = select i1 %1, i256 42, i256 %v2
  ret i256 %2
}

; CHECK-LABEL: selcrr
define i256 @selcrr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ugt i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  ; CHECK: add r2, r0, r1
  ; CHECK: add.gt @val[0], r0, r1
  %2 = select i1 %1, i256 %const, i256 %v2
  ret i256 %2
}

; CHECK-LABEL: selsrr
define i256 @selsrr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %data = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ugt i256 %v3, %v4
  %val = load i256, i256* %data
  ; CHECK: add r2, r0, r1
  ; CHECK: add.gt stack-[1], r0, r1
  %2 = select i1 %1, i256 %val, i256 %v2
  ret i256 %2
}

; CHECK-LABEL: selrir
define i256 @selrir(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  ; CHECK: add.ge 42, r0, r1
  %2 = select i1 %1, i256 %v1, i256 42
  ret i256 %2
}

; CHECK-LABEL: seliir
define i256 @seliir(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  ; CHECK: add 42, r0, r1
  ; CHECK: add.lt 17, r0, r1
  %2 = select i1 %1, i256 17, i256 42
  ret i256 %2
}

; CHECK-LABEL: selcir
define i256 @selcir(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ugt i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  ; CHECK: add 42, r0, r1
  ; CHECK: add.gt @val[0], r0, r1
  %2 = select i1 %1, i256 %const, i256 42
  ret i256 %2
}

; CHECK-LABEL: selsir
define i256 @selsir(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %data = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ugt i256 %v3, %v4
  %val = load i256, i256* %data
  ; CHECK: add 42, r0, r1
  ; CHECK: add.gt stack-[1], r0, r1
  %2 = select i1 %1, i256 %val, i256 42
  ret i256 %2
}

; CHECK-LABEL: selrcr
define i256 @selrcr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  ; CHECK: add.ge @val[0], r0, r1
  %2 = select i1 %1, i256 %v1, i256 %const
  ret i256 %2
}

; CHECK-LABEL: selicr
define i256 @selicr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  ; CHECK: add @val[0], r0, r1
  ; CHECK: add.lt 42, r0, r1
  %2 = select i1 %1, i256 42, i256 %const
  ret i256 %2
}

; CHECK-LABEL: selccr
define i256 @selccr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  %const2 = load i256, i256 addrspace(4)* @val2
  ; CHECK: add @val[0], r0, r1
  ; CHECK: add.lt @val2[0], r0, r1
  %2 = select i1 %1, i256 %const2, i256 %const
  ret i256 %2
}

; CHECK-LABEL: selscr
define i256 @selscr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %ptr = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  %val2 = load i256, i256* %ptr
  ; CHECK: add @val[0], r0, r1
  ; CHECK: add.lt stack-[1], r0, r1
  %2 = select i1 %1, i256 %val2, i256 %const
  ret i256 %2
}

; CHECK-LABEL: selrsr
define i256 @selrsr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %data = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %val = load i256, i256* %data
  ; CHECK: add.ge stack-[1], r0, r1
  %2 = select i1 %1, i256 %v1, i256 %val
  ret i256 %2
}

; CHECK-LABEL: selisr
define i256 @selisr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %data = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %val = load i256, i256* %data
  ; CHECK: add stack-[1], r0, r1
  ; CHECK: add.lt 42, r0, r1
  %2 = select i1 %1, i256 42, i256 %val
  ret i256 %2
}

; CHECK-LABEL: selcsr
define i256 @selcsr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %data = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %val = load i256, i256* %data
  %const = load i256, i256 addrspace(4)* @val
  ; CHECK: add stack-[1], r0, r1
  ; CHECK: add.lt @val[0], r0, r1
  %2 = select i1 %1, i256 %const, i256 %val
  ret i256 %2
}

; CHECK-LABEL: selssr
define i256 @selssr(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %data1 = alloca i256
  %data2 = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %val = load i256, i256* %data1
  %val2 = load i256, i256* %data2
  ; CHECK: add stack-[2], r0, r1
  ; CHECK: add.lt stack-[1], r0, r1
  %2 = select i1 %1, i256 %val2, i256 %val
  ret i256 %2
}

; CHECK-LABEL: selrrs
define void @selrrs(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp eq i256 %v3, %v4
  ; TODO: convert to add.ne if VM support
  ; CHECK: add r2, r0, stack-[1]
  ; CHECK: add.eq r1, r0, stack-[1]
  %2 = select i1 %1, i256 %v1, i256 %v2
  store i256 %2, i256* %resptr
  ret void
}


; CHECK-LABEL: selirs
define void @selirs(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ugt i256 %v3, %v4
  ; CHECK: add r2, r0, stack-[1]
  ; CHECK: add.gt 42, r0, stack-[1]
  %2 = select i1 %1, i256 42, i256 %v2
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selcrs
define void @selcrs(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ugt i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  ; CHECK: add r2, r0, stack-[1]
  ; CHECK: add.gt @val[0], r0, stack-[1]
  %2 = select i1 %1, i256 %const, i256 %v2
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selsrs
define void @selsrs(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  %data = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ugt i256 %v3, %v4
  %val = load i256, i256* %data
  ; CHECK: add r2, r0, stack-[2]
  ; CHECK: add.gt stack-[1], r0, stack-[2]
  %2 = select i1 %1, i256 %val, i256 %v2
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selris
define void @selris(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  ; CHECK: add 42, r0, stack-[1]
  ; CHECK: add.lt r1, r0, stack-[1]
  %2 = select i1 %1, i256 %v1, i256 42
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: seliis
define void @seliis(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  ; CHECK: add 42, r0, stack-[1]
  ; CHECK: add.lt 17, r0, stack-[1]
  %2 = select i1 %1, i256 17, i256 42
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selcis
define void @selcis(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ugt i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  ; CHECK: add 42, r0, stack-[1]
  ; CHECK: add.gt @val[0], r0, stack-[1]
  %2 = select i1 %1, i256 %const, i256 42
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selsis
define void @selsis(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  %data = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ugt i256 %v3, %v4
  %val = load i256, i256* %data
  ; CHECK: add 42, r0, stack-[2]
  ; CHECK: add.gt stack-[1], r0, stack-[2]
  %2 = select i1 %1, i256 %val, i256 42
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selrcs
define void @selrcs(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  ; CHECK: add @val[0], r0, stack-[1]
  ; CHECK: add.lt r1, r0, stack-[1]
  %2 = select i1 %1, i256 %v1, i256 %const
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selics
define void @selics(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  ; CHECK: add @val[0], r0, stack-[1]
  ; CHECK: add.lt 42, r0, stack-[1]
  %2 = select i1 %1, i256 42, i256 %const
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selccs
define void @selccs(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  ; TODO: should be materialized in add.
	; CHECK: add @val[0], r0, r1
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  %const2 = load i256, i256 addrspace(4)* @val2
  ; CHECK: add r1, r0, stack-[1]
  ; CHECK: add.lt @val2[0], r0, stack-[1]
  %2 = select i1 %1, i256 %const2, i256 %const
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selscs
define void @selscs(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  %ptr = alloca i256
  ; TODO: should be materialized in add
	; CHECK: add	@val[0], r0, r1
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %const = load i256, i256 addrspace(4)* @val
  %val2 = load i256, i256* %ptr
  ; CHECK: add r1, r0, stack-[2]
  ; CHECK: add.lt stack-[1], r0, stack-[2]
  %2 = select i1 %1, i256 %val2, i256 %const
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selrss
define void @selrss(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  %data = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %val = load i256, i256* %data
  ; CHECK: add stack-[1], r0, stack-[2]
  ; CHECK: add.lt r1, r0, stack-[2]
  %2 = select i1 %1, i256 %v1, i256 %val
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: seliss
define void @seliss(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  %data = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %val = load i256, i256* %data
  ; CHECK: add stack-[1], r0, stack-[2]
  ; CHECK: add.lt 42, r0, stack-[2]
  %2 = select i1 %1, i256 42, i256 %val
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selcss
define void @selcss(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  %data = alloca i256
  ; TODO: should be materialized in add
	; CHECK: add	@val[0], r0, r1
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %val = load i256, i256* %data
  %const = load i256, i256 addrspace(4)* @val
  ; CHECK: add stack-[1], r0, stack-[2]
  ; CHECK: add.lt r1, r0, stack-[2]
  %2 = select i1 %1, i256 %const, i256 %val
  store i256 %2, i256* %resptr
  ret void
}

; CHECK-LABEL: selsss
define void @selsss(i256 %v1, i256 %v2, i256 %v3, i256 %v4) {
  %resptr = alloca i256
  %data1 = alloca i256
  %data2 = alloca i256
  ; CHECK: sub! r3, r4, r{{[0-9]+}}
  %1 = icmp ult i256 %v3, %v4
  %val = load i256, i256* %data1
  %val2 = load i256, i256* %data2
  ; CHECK: add stack-[2], r0, r1
  ; CHECK: add.lt stack-[1], r0, r1
  %2 = select i1 %1, i256 %val2, i256 %val
  store i256 %2, i256* %resptr
  ret void
}
