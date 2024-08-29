; RUN: llc --disable-eravm-scalar-opt-passes < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = addrspace(4) global i256 42
declare i256 @foo()
declare i8 addrspace(3)* @bar()

; ============================ Binary instructions =============================

; CHECK-LABEL: spill_addr
define i256 @spill_addr(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: add stack-[1], r1, r1
  %res = add i256 %a, %b
  ret i256 %res
}

; CHECK-LABEL: spill_adds
define void @spill_adds(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  ; CHECK: add stack-[1], r1, stack-[2] 
  %res = add i256 %a, %b
  store i256 %res, i256* %slot
  ret void
}

; CHECK-LABEL: spill_subr
define i256 @spill_subr(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: sub stack-[1], r1, r1
  %res = sub i256 %a, %b
  ret i256 %res
}


; CHECK-LABEL: spill_subs
define void @spill_subs(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  ; CHECK: sub stack-[1], r1, stack-[2] 
  %res = sub i256 %a, %b
  store i256 %res, i256* %slot
  ret void
}

; CHECK-LABEL: spill_mulr
define i256 @spill_mulr(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: mul stack-[1], r1, r1, r0
  %res = mul i256 %a, %b
  ret i256 %res
}

; CHECK-LABEL: spill_muls
define void @spill_muls(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  ; CHECK: mul stack-[1], r1, stack-[2], r0
  %res = mul i256 %a, %b
  store i256 %res, i256* %slot
  ret void
}

; CHECK-LABEL: spill_mulhr
define i256 @spill_mulhr(i256 %a) nounwind {
  %b = call i256 @foo()
  %aext = zext i256 %a to i512
  %bext = zext i256 %b to i512
  %resl = mul i512 %aext, %bext
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
  ; CHECK: mul stack-[1], r1, r0, r1
  ret i256 %res2
}

; CHECK-LABEL: spill_mulhs
define void @spill_mulhs(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  %aext = zext i256 %a to i512
  %bext = zext i256 %b to i512
  %resl = mul i512 %aext, %bext
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
  ; CHECK: mul stack-[1], r1, r0, r1
  ; CHECK: add r1, r0, stack-[2]
  store i256 %res2, i256* %slot
  ret void
}

; CHECK-LABEL: spill_divr
define i256 @spill_divr(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: div stack-[1], r1, r1, r0
  %res = udiv i256 %a, %b
  ret i256 %res
}

; CHECK-LABEL: spill_divs
define void @spill_divs(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  ; CHECK: div stack-[1], r1, stack-[2], r0
  %res = udiv i256 %a, %b
  store i256 %res, i256* %slot
  ret void
}

; CHECK-LABEL: spill_remr
define i256 @spill_remr(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: div stack-[1], r1, r0, r1
  %res = urem i256 %a, %b
  ret i256 %res
}

; CHECK-LABEL: spill_rems
define void @spill_rems(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  ; CHECK: div stack-[1], r1, r0, r1
  ; CHECK: add r1, r0, stack-[2]
  %res = urem i256 %a, %b
  store i256 %res, i256* %slot
  ret void
}

; CHECK-LABEL: spill_andr
define i256 @spill_andr(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: and stack-[1], r1, r1
  %res = and i256 %a, %b
  ret i256 %res
}

; CHECK-LABEL: spill_ands
define void @spill_ands(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  ; CHECK: and stack-[1], r1, stack-[2] 
  %res = and i256 %a, %b
  store i256 %res, i256* %slot
  ret void
}

; CHECK-LABEL: spill_orr
define i256 @spill_orr(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: or stack-[1], r1, r1
  %res = or i256 %a, %b
  ret i256 %res
}

; CHECK-LABEL: spill_ors
define void @spill_ors(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  ; CHECK: or stack-[1], r1, stack-[2] 
  %res = or i256 %a, %b
  store i256 %res, i256* %slot
  ret void
}

; CHECK-LABEL: spill_xorr
define i256 @spill_xorr(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: xor stack-[1], r1, r1
  %res = xor i256 %a, %b
  ret i256 %res
}

; CHECK-LABEL: spill_xors
define void @spill_xors(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  ; CHECK: xor stack-[1], r1, stack-[2] 
  %res = xor i256 %a, %b
  store i256 %res, i256* %slot
  ret void
}

; CHECK-LABEL: spill_shlr
define i256 @spill_shlr(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: shl stack-[1], r1, r1
  %res = shl i256 %a, %b
  ret i256 %res
}

; CHECK-LABEL: spill_shls
define void @spill_shls(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  ; CHECK: shl stack-[1], r1, stack-[2] 
  %res = shl i256 %a, %b
  store i256 %res, i256* %slot
  ret void
}

; CHECK-LABEL: spill_shrr
define i256 @spill_shrr(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: shr stack-[1], r1, r1
  %res = lshr i256 %a, %b
  ret i256 %res
}

; CHECK-LABEL: spill_shrs
define void @spill_shrs(i256 %a) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
  ; CHECK: shr stack-[1], r1, stack-[2] 
  %res = lshr i256 %a, %b
  store i256 %res, i256* %slot
  ret void
}

; CHECK-LABEL: spill_ptraddr
define i8 addrspace(3)* @spill_ptraddr(i8 addrspace(3)* %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: addp stack-[1], r1, r1
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %a, i256 %b)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptradds
define void @spill_ptradds(i8 addrspace(3)* %a) nounwind {
  %slot = alloca i8 addrspace(3)*
  %b = call i256 @foo()
  ; CHECK: addp stack-[1], r1, stack-[2]
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %a, i256 %b)
  store i8 addrspace(3)* %res, i8 addrspace(3)** %slot
  ret void
}

; CHECK-LABEL: spill_ptrpackr
define i8 addrspace(3)* @spill_ptrpackr(i8 addrspace(3)* %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: pack stack-[1], r1, r1
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %a, i256 %b)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrpacks
define void @spill_ptrpacks(i8 addrspace(3)* %a) nounwind {
  %slot = alloca i8 addrspace(3)*
  %b = call i256 @foo()
  ; CHECK: pack stack-[1], r1, stack-[2]
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %a, i256 %b)
  store i8 addrspace(3)* %res, i8 addrspace(3)** %slot
  ret void
}

; CHECK-LABEL: spill_ptrshrinkr
define i8 addrspace(3)* @spill_ptrshrinkr(i8 addrspace(3)* %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: shrnk stack-[1], r1, r1
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %a, i256 %b)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrshrinks
define void @spill_ptrshrinks(i8 addrspace(3)* %a) nounwind {
  %slot = alloca i8 addrspace(3)*
  %b = call i256 @foo()
  ; CHECK: shrnk stack-[1], r1, stack-[2]
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %a, i256 %b)
  store i8 addrspace(3)* %res, i8 addrspace(3)** %slot
  ret void
}

; ========================= Binary instructions (In1) ==========================

; CHECK-LABEL: spill_addrc
define i256 @spill_addrc(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: add stack-[1], r1, r1
  %res = add i256 %b, %a
  ret i256 %res
}

; CHECK-LABEL: spill_subrx
define i256 @spill_subrx(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: sub.s stack-[1], r1, r1
  %res = sub i256 %b, %a
  ret i256 %res
}

; CHECK-LABEL: spill_mulx
define i256 @spill_mulx(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: mul stack-[1], r1, r1, r0
  %res = mul i256 %b, %a
  ret i256 %res
}

; CHECK-LABEL: spill_mulhx
define i256 @spill_mulhx(i256 %a) nounwind {
  %b = call i256 @foo()
  %aext = zext i256 %a to i512
  %bext = zext i256 %b to i512
  %resl = mul i512 %bext, %aext
  %res2l = lshr i512 %resl, 256
  %res2 = trunc i512 %res2l to i256
  ; CHECK: mul stack-[1], r1, r0, r1
  ret i256 %res2
}

; CHECK-LABEL: spill_divx
define i256 @spill_divx(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: div.s stack-[1], r1, r1, r0
  %res = udiv i256 %b, %a
  ret i256 %res
}

; CHECK-LABEL: spill_remx
define i256 @spill_remx(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: div.s stack-[1], r1, r0, r1
  %res = urem i256 %b, %a
  ret i256 %res
}

; CHECK-LABEL: spill_andx
define i256 @spill_andx(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: and stack-[1], r1, r1
  %res = and i256 %b, %a
  ret i256 %res
}

; CHECK-LABEL: spill_orx
define i256 @spill_orx(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: or stack-[1], r1, r1
  %res = or i256 %b, %a
  ret i256 %res
}

; CHECK-LABEL: spill_xorx
define i256 @spill_xorx(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: xor stack-[1], r1, r1
  %res = xor i256 %b, %a
  ret i256 %res
}

; CHECK-LABEL: spill_shlx
define i256 @spill_shlx(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: shl.s stack-[1], r1, r1
  %res = shl i256 %b, %a
  ret i256 %res
}

; CHECK-LABEL: spill_shrx
define i256 @spill_shrx(i256 %a) nounwind {
  %b = call i256 @foo()
  ; CHECK: shr.s stack-[1], r1, r1
  %res = lshr i256 %b, %a
  ret i256 %res
}

; CHECK-LABEL: spill_ptraddx
define i8 addrspace(3)* @spill_ptraddx(i256 %a) nounwind {
  %b = call i8 addrspace(3)* @bar()
  ; CHECK: addp.s stack-[1], r1, r1
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %b, i256 %a)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrpackx
define i8 addrspace(3)* @spill_ptrpackx(i256 %a) nounwind {
  %b = call i8 addrspace(3)* @bar()
  ; CHECK: pack.s stack-[1], r1, r1
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %b, i256 %a)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrshrinkx
define i8 addrspace(3)* @spill_ptrshrinkx(i256 %a) nounwind {
  %b = call i8 addrspace(3)* @bar()
  ; CHECK: shrnk.s stack-[1], r1, r1
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %b, i256 %a)
  ret i8 addrspace(3)* %res
}

; ================================ Flag setting ================================
; CHECK-LABEL: spill_subv
define i256 @spill_subv(i256 %a, i256 %b, i256 %c) nounwind {
  %d = call i256 @foo()
  %cmp = icmp ugt i256 %c, %d
  ; CHECK: sub!	stack-[2], r1, r0
  %res = select i1 %cmp, i256 %a, i256 %b
  ret i256 %res
}

; CHECK-LABEL: spill_andv
define i256 @spill_andv(i256 %a, i256 %b, i256 %c) nounwind {
  %d = call i256 @foo()
  %andres = and i256 %c, %d
  %cmp = icmp eq i256 %andres, 0
  ; CHECK: and!	stack-[2], r1, r0
  %res = select i1 %cmp, i256 %a, i256 %b
  ret i256 %res
}

; ================================== Select ====================================
; CHECK-LABEL: spill_select
define i256 @spill_select(i256 %a, i1 %cond) nounwind {
  %b = call i256 @foo()
; CHECK: sub! stack-[1], r0, r0
; CHECK: add.ne stack-[2], r0, r2
  %res = select i1 %cond, i256 %a, i256 %b
  ret i256 %res
}

; CHECK-LABEL: spill_selrrs1
define void @spill_selrrs1(i256 %a, i1 %cond) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
; CHECK: sub! stack-[1], r0, r0
; CHECK: add r1, r0, stack-[3]
; CHECK: add.ne stack-[2], r0, stack-[3]
  %sel = select i1 %cond, i256 %a, i256 %b
  store i256 %sel, i256* %slot
  ret void
}

; CHECK-LABEL: spill_selrrs2
define void @spill_selrrs2(i256 %a, i1 %cond) nounwind {
  %slot = alloca i256
  %b = call i256 @foo()
; CHECK: sub! stack-[1], r0, r0
; CHECK: add stack-[2], r0, stack-[3]
; CHECK: add.ne r1, r0, stack-[3]
  %sel = select i1 %cond, i256 %b, i256 %a
  store i256 %sel, i256* %slot
  ret void
}

; ==============================================================================
; CHECK-LABEL: spill_multiple_use
define void @spill_multiple_use(i256 %a) nounwind {
  %slot = alloca i256
  %slot2 = alloca i256
  %b = call i256 @foo()
  %res = and i256 %a, %b
; CHECK: and stack-[1], r1, stack-[3]
; CHECK: and stack-[1], r1, stack-[2]
  store i256 %res, i256* %slot
  store i256 %res, i256* %slot2
  ret void
}

; CHECK-LABEL: swap
define void @swap(i256 %a) nounwind {
; CHECK: add stack-[1], r0, r1
; CHECK: add stack-[2], r0, stack-[1]
; CHECK: add r1, r0, stack-[2]
  %slot1 = alloca i256
  %slot2 = alloca i256
  %s1val = load i256, i256* %slot1
  %s2val = load i256, i256* %slot2
  store i256 %s1val, i256* %slot2
  store i256 %s2val, i256* %slot1
  ret void
}

; CHECK-LABEL: swapinv
define void @swapinv(i256 %a) nounwind {
; CHECK: add stack-[2], r0, r2
; CHECK: sub.s stack-[1], r1, stack-[2]
; CHECK: sub r1, r2, stack-[1]
  %slot1 = alloca i256
  %slot2 = alloca i256
  %s1val = load i256, i256* %slot1
  %s2val = load i256, i256* %slot2
  %si1val = sub i256 %a, %s1val
  %si2val = sub i256 %a, %s2val
  store i256 %si1val, i256* %slot2
  store i256 %si2val, i256* %slot1
  ret void
}

; CHECK-LABEL: dont_combine_predicated_use
define i256 @dont_combine_predicated_use(i256 %a, i1 %cond) nounwind {
  %slot1 = alloca i256
  %b = call i256 @foo()
  %s1val = load i256, i256* %slot1
  br i1 %cond, label %ltrue, label %lfalse
; CHECK: add stack-[2], r0, r2
; CHECK: sub! stack-[1], r0, r0
; TODO: CPR-1367 In that particular case we can combine, but it isn't analyzed in the pass.
; CHECK: add.eq r2, r0, r1
; CHECK: add.ne r2, r1, r1
ltrue:
  %res = add i256 %s1val, %b
  ret i256 %res
lfalse:
  ret i256 %s1val
}

declare i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)*, i256)
declare i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)*, i256)
declare i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)*, i256)
