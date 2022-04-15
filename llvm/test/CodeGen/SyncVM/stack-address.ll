; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: alloca
define void @alloca(i256 %a1, i256 %a2, i256 %a3, i256 %a4) nounwind {
  ; CHECK: nop stack+=[4]
  %var1 = alloca i256
  %var2 = alloca i256
  %var3 = alloca i256
  %var4 = alloca i256
  ; CHECK-DAG: add r1, r0, stack-[4]
  ; CHECK-DAG: add r2, r0, stack-[3]
  ; CHECK-DAG: add r3, r0, stack-[2]
  ; CHECK-DAG: add r4, r0, stack-[1]
  store i256 %a1, i256* %var1, align 32
  store i256 %a2, i256* %var2, align 32
  store i256 %a3, i256* %var3, align 32
  store i256 %a4, i256* %var4, align 32
  ; CHECK: nop stack-=[4]
  ret void
}

; CHECK-LABEL: stack.obj.accessing
define i256 @stack.obj.accessing(i256* %ptr.i256, [4 x i256]* %ptr.arr, i256* %ptr.elem) {
  %gep0 = getelementptr [4 x i256], [4 x i256]* %ptr.arr, i256 0, i256 0
  %gep1 = getelementptr [4 x i256], [4 x i256]* %ptr.arr, i256 0, i256 1
  %gep2 = getelementptr [4 x i256], [4 x i256]* %ptr.arr, i256 0, i256 2
  %gep3 = getelementptr [4 x i256], [4 x i256]* %ptr.arr, i256 0, i256 3
; CHECK: div.s 32, r2, r4, r0
; CHECK: add stack[r4 - 0], r0, r4
; CHECK: div.s 32, r1, r1, r0
; CHECK: add stack[r1 - 0], r4, r1
; CHECK: div.s 32, r2, r4, r0
; CHECK: add stack[r4 + 1], r1, r1
; CHECK: div.s 32, r2, r4, r0
; CHECK: add stack[r4 + 2], r1, r1
; CHECK: div.s 32, r2, r2, r0
; CHECK: add stack[r2 + 3], r1, r1
; CHECK: div.s 32, r3, r2, r0
; CHECK: add stack[r2 - 0], r1, r1
  %v1 = load i256, i256* %ptr.i256
  %v2 = load i256, i256* %gep0
  %v3 = load i256, i256* %gep1
  %v4 = load i256, i256* %gep2
  %v5 = load i256, i256* %gep3
  %v6 = load i256, i256* %ptr.elem
  %sum0 = add i256 %v1, %v2
  %sum1 = add i256 %sum0, %v3
  %sum2 = add i256 %sum1, %v4
  %sum3 = add i256 %sum2, %v5
  %sum4 = add i256 %sum3, %v6
  ret i256 %sum4
}

; CHECK-LABEL: stack.obj.passing
define i256 @stack.obj.passing() {
; CHECK: nop stack+=[12]
  %unused1 = alloca [3 x i256]
  %par1 = alloca i256
  %unused2 = alloca [2 x i256]
  %elem.cont = alloca [2 x i256]
  %arr = alloca [4 x i256]
  %elem = getelementptr [2 x i256], [2 x i256]* %elem.cont, i256 0, i256 0
; CHECK: context.sp r1
; CHECK: sub.s 9, r1, r1
; CHECK: mul 32, r1, r1, r0
; CHECK: context.sp r2
; CHECK: sub.s 4, r2, r2
; CHECK: mul 32, r2, r2, r0
; CHECK: context.sp r3
; CHECK: sub.s 6, r3, r3
; CHECK: mul 32, r3, r3, r0
; CHECK: near_call	r0, @stack.obj.accessing, @DEFAULT_UNWIND
  %res = call i256 @stack.obj.accessing(i256* %par1, [4 x i256]* %arr, i256* %elem)
; CHECK: nop stack-=[12]
  ret i256 %res
}

