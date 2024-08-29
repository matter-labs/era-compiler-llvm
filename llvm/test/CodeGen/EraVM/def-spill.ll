; RUN: llc --disable-eravm-scalar-opt-passes < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare i256 @foo()
declare i8 addrspace(3)* @bar()

; ============================ Binary instructions =============================

; CHECK-LABEL: spill_addr
define i256 @spill_addr(i256 %a, i256 %b) nounwind {
  ; CHECK: add r1, r2, stack-[1]
  %x = add i256 %a, %b
  %c = call i256 @foo()
  %res = add i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_addr_selirs_use
define i256 @spill_addr_selirs_use(i256 %a, i256 %b, i1 %cond) nounwind {
  %slot = alloca i256
  ; CHECK: add r1, r2, stack-[1]
  %x = add i256 %a, %b
  ; CHECK: sub! r3, r0, r0
  ; CHECK: add stack-[1], r0, stack-[2]
  ; CHECK: add.ne 1234, r0, stack-[2]
  %sel = select i1 %cond, i256 1234, i256 %x
  store i256 %sel, i256* %slot
  %c = call i256 @foo()
  %res = add i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_addr_selris_use
define i256 @spill_addr_selris_use(i256 %a, i256 %b, i1 %cond) nounwind {
  %slot = alloca i256
  ; CHECK: add r1, r2, stack-[1]
  %x = add i256 %a, %b
  ; CHECK: sub! r3, r0, r0
  ; CHECK: add 1234, r0, stack-[2]
  ; CHECK: add.ne stack-[1], r0, stack-[2]
  %sel = select i1 %cond, i256 %x, i256 1234
  store i256 %sel, i256* %slot
  %c = call i256 @foo()
  %res = add i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_addi
define i256 @spill_addi(i256 %a) nounwind {
  ; TODO: CPR-1221 add 42, r2, stack-[1]
  %x = add i256 %a, 42
  %c = call i256 @foo()
  %res = add i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_addc
define i256 @spill_addc(i256 %a) nounwind {
  ; TODO: CPR-1221 add code[@CPI2_0], r2, stack-[1]
  %x = add i256 %a, 4200000000000000
  %c = call i256 @foo()
  %res = add i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_adds
define i256 @spill_adds(i256 %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: add stack-[2], r1, stack-[1]
  %x = add i256 %a, %b
  %c = call i256 @foo()
  %res = add i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_subr
define i256 @spill_subr(i256 %a, i256 %b) nounwind {
  ; CHECK: sub r1, r2, stack-[1]
  %x = sub i256 %a, %b
  %c = call i256 @foo()
  %res = sub i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_subi
define i256 @spill_subi(i256 %a) nounwind {
  ; TODO: CPR-1221 sub 42, r2, stack-[1]
  %x = sub i256 %a, 42
  %c = call i256 @foo()
  %res = sub i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_subc
define i256 @spill_subc(i256 %a) nounwind {
  ; TODO: CPR-1221 sub code[@CPI2_0], r2, stack-[1]
  %x = sub i256 %a, 4200000000000000
  %c = call i256 @foo()
  %res = sub i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_subs
define i256 @spill_subs(i256 %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: sub.s stack-[2], r1, stack-[1]
  %x = sub i256 %a, %b
  %c = call i256 @foo()
  %res = sub i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_mulr
define i256 @spill_mulr(i256 %a, i256 %b) nounwind {
  ; CHECK: mul r1, r2, stack-[1], r0
  %x = mul i256 %a, %b
  %c = call i256 @foo()
  %res = mul i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_muli
define i256 @spill_muli(i256 %a) nounwind {
  ; TODO: CPR-1221 mul 42, r2, stack-[1]
  %x = mul i256 %a, 42
  %c = call i256 @foo()
  %res = mul i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_mulc
define i256 @spill_mulc(i256 %a) nounwind {
  ; TODO: CPR-1221 mul code[@CPI2_0], r2, stack-[1]
  %x = mul i256 %a, 4200000000000000
  %c = call i256 @foo()
  %res = mul i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_muls
define i256 @spill_muls(i256 %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: mul stack-[2], r1, stack-[1], r0
  %x = mul i256 %a, %b
  %c = call i256 @foo()
  %res = mul i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_divr
define i256 @spill_divr(i256 %a, i256 %b) nounwind {
  ; CHECK: div r1, r2, stack-[1], r0
  %x = udiv i256 %a, %b
  %c = call i256 @foo()
  %res = udiv i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_divi
define i256 @spill_divi(i256 %a) nounwind {
  ; CHECK: div.s 42, r1, stack-[1], r0
  %x = udiv i256 %a, 42
  %c = call i256 @foo()
  %res = udiv i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_divc
define i256 @spill_divc(i256 %a) nounwind {
  ; CHECK: div.s code[@CPI{{[0-9]+}}_0], r1, stack-[1], r0
  %x = udiv i256 %a, 4200000000000000
  %c = call i256 @foo()
  %res = udiv i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_divs
define i256 @spill_divs(i256 %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: div.s stack-[2], r1, stack-[1], r0
  %x = udiv i256 %a, %b
  %c = call i256 @foo()
  %res = udiv i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_andr
define i256 @spill_andr(i256 %a, i256 %b) nounwind {
  ; CHECK: and r1, r2, stack-[1]
  %x = and i256 %a, %b
  %c = call i256 @foo()
  %res = and i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_andi
define i256 @spill_andi(i256 %a) nounwind {
  ; TODO: CPR-1221 and 42, r2, stack-[1]
  %x = and i256 %a, 42
  %c = call i256 @foo()
  %res = and i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_andc
define i256 @spill_andc(i256 %a) nounwind {
  ; TODO: CPR-1221 and code[@CPI2_0], r2, stack-[1]
  %x = and i256 %a, 4200000000000000
  %c = call i256 @foo()
  %res = and i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_ands
define i256 @spill_ands(i256 %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: and stack-[2], r1, stack-[1]
  %x = and i256 %a, %b
  %c = call i256 @foo()
  %res = and i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_orr
define i256 @spill_orr(i256 %a, i256 %b) nounwind {
  ; CHECK: or r1, r2, stack-[1]
  %x = or i256 %a, %b
  %c = call i256 @foo()
  %res = or i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_ori
define i256 @spill_ori(i256 %a) nounwind {
  ; TODO: CPR-1221 or 42, r2, stack-[1]
  %x = or i256 %a, 42
  %c = call i256 @foo()
  %res = or i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_orc
define i256 @spill_orc(i256 %a) nounwind {
  ; TODO: CPR-1221 or code[@CPI2_0], r2, stack-[1]
  %x = or i256 %a, 4200000000000000
  %c = call i256 @foo()
  %res = or i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_ors
define i256 @spill_ors(i256 %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: or stack-[2], r1, stack-[1]
  %x = or i256 %a, %b
  %c = call i256 @foo()
  %res = or i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_xorr
define i256 @spill_xorr(i256 %a, i256 %b) nounwind {
  ; CHECK: xor r1, r2, stack-[1]
  %x = xor i256 %a, %b
  %c = call i256 @foo()
  %res = xor i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_xori
define i256 @spill_xori(i256 %a) nounwind {
  ; TODO: CPR-1221 xor 42, r2, stack-[1]
  %x = xor i256 %a, 42
  %c = call i256 @foo()
  %res = xor i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_xorc
define i256 @spill_xorc(i256 %a) nounwind {
  ; TODO: CPR-1221 xor code[@CPI2_0], r2, stack-[1]
  %x = xor i256 %a, 4200000000000000
  %c = call i256 @foo()
  %res = xor i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_xors
define i256 @spill_xors(i256 %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: xor stack-[2], r1, stack-[1]
  %x = xor i256 %a, %b
  %c = call i256 @foo()
  %res = xor i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_shlr
define i256 @spill_shlr(i256 %a, i256 %b) nounwind {
  ; CHECK: shl r1, r2, stack-[1]
  %x = shl i256 %a, %b
  %c = call i256 @foo()
  %res = shl i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_shli
define i256 @spill_shli(i256 %a) nounwind {
  ; CHECK: shl.s 42, r1, stack-[1]
  %x = shl i256 %a, 42
  %c = call i256 @foo()
  %res = shl i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_shls
define i256 @spill_shls(i256 %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: shl.s stack-[2], r1, stack-[1]
  %x = shl i256 %a, %b
  %c = call i256 @foo()
  %res = shl i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_shrr
define i256 @spill_shrr(i256 %a, i256 %b) nounwind {
  ; CHECK: shr r1, r2, stack-[1]
  %x = lshr i256 %a, %b
  %c = call i256 @foo()
  %res = lshr i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_shri
define i256 @spill_shri(i256 %a) nounwind {
  ; CHECK: shr.s 42, r1, stack-[1]
  %x = lshr i256 %a, 42
  %c = call i256 @foo()
  %res = lshr i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_shrs
define i256 @spill_shrs(i256 %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: shr.s stack-[2], r1, stack-[1]
  %x = lshr i256 %a, %b
  %c = call i256 @foo()
  %res = lshr i256 %x, %c
  ret i256 %res
}

; CHECK-LABEL: spill_ptraddr
define i8 addrspace(3)* @spill_ptraddr(i8 addrspace(3)* %a, i256 %b) nounwind {
  ; CHECK: addp r1, r2, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %a, i256 %b)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptraddi
define i8 addrspace(3)* @spill_ptraddi(i8 addrspace(3)* %a) nounwind {
  ; CHECK: addp.s 42, r1, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %a, i256 42)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptraddc
define i8 addrspace(3)* @spill_ptraddc(i8 addrspace(3)* %a) nounwind {
  ; CHECK: addp.s code[@CPI{{[0-9]+}}_0], r1, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %a, i256 4200000000000000)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptradds
define i8 addrspace(3)* @spill_ptradds(i8 addrspace(3)* %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: addp.s stack-[2], r1, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %a, i256 %b)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrpackr
define i8 addrspace(3)* @spill_ptrpackr(i8 addrspace(3)* %a, i256 %b) nounwind {
  ; CHECK: pack r1, r2, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %a, i256 %b)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrpacki
define i8 addrspace(3)* @spill_ptrpacki(i8 addrspace(3)* %a) nounwind {
  ; CHECK: pack.s 42, r1, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %a, i256 42)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrpackc
define i8 addrspace(3)* @spill_ptrpackc(i8 addrspace(3)* %a) nounwind {
  ; CHECK: pack.s code[@CPI{{[0-9]+}}_0], r1, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %a, i256 4200000000000000)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrpacks
define i8 addrspace(3)* @spill_ptrpacks(i8 addrspace(3)* %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: pack.s stack-[2], r1, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %a, i256 %b)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrshrinkr
define i8 addrspace(3)* @spill_ptrshrinkr(i8 addrspace(3)* %a, i256 %b) nounwind {
  ; CHECK: shrnk r1, r2, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %a, i256 %b)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrshrinki
define i8 addrspace(3)* @spill_ptrshrinki(i8 addrspace(3)* %a) nounwind {
  ; CHECK: shrnk.s 42, r1, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %a, i256 42)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrshrinkc
define i8 addrspace(3)* @spill_ptrshrinkc(i8 addrspace(3)* %a) nounwind {
  ; CHECK: shrnk.s code[@CPI{{[0-9]+}}_0], r1, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %a, i256 4200000000000000)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; CHECK-LABEL: spill_ptrshrinks
define i8 addrspace(3)* @spill_ptrshrinks(i8 addrspace(3)* %a) nounwind {
  %slot = alloca i256
  %b = load i256, i256* %slot
  ; CHECK: shrnk.s stack-[2], r1, stack-[1]
  %x = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %a, i256 %b)
  %c = call i256 @foo()
  %res = call i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)* %x, i256 %c)
  ret i8 addrspace(3)* %res
}

; ============================ Multiple uses =============================

; CHECK-LABEL: spill_multiple
define i256 @spill_multiple(i256 %a, i256 %b, i256 %c) nounwind {
  %slot = alloca i256
; CHECK: add r1, r2, stack-[1]
; CHECK: add stack-[1], r3, stack-[2]
  %x = add i256 %a, %b
  %y = add i256 %x, %c
  store i256 %y, i256* %slot
  %d = call i256 @foo()
  %res = add i256 %x, %d
  ret i256 %res
}

; ============================ Other issues ==============================

; Spilled out0 must be checked for equality against instructions in between def and spill, not the def one.
; CHECK-LABEL: check_spilled_out0
define i256 @check_spilled_out0(i256 %a) nounwind {
; CHECK: add stack-[1], r0, r1
; CHECK: add stack-[3], r0, r2
; CHECK: add r2, r0, stack-[1]
; prevents combining to add stack-[3], r0, stack-[2]
; CHECK: add stack-[2], r1, r1
; CHECK: add r2, r0, stack-[2]
  %slot1 = alloca i256
  %slot2 = alloca i256
  %slot3 = alloca i256
  %s1val = load i256, i256* %slot1
  %s2val = load i256, i256* %slot2
  %s3val = load i256, i256* %slot3
  %res = add i256 %s2val, %s3val
  store i256 %s1val, i256* %slot3
  store i256 %s1val, i256* %slot2
  ret i256 %res
}

declare i8 addrspace(3)* @llvm.eravm.ptr.add(i8 addrspace(3)*, i256)
declare i8 addrspace(3)* @llvm.eravm.ptr.pack(i8 addrspace(3)*, i256)
declare i8 addrspace(3)* @llvm.eravm.ptr.shrink(i8 addrspace(3)*, i256)
