; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@val = global [10 x i256] zeroinitializer
@const = addrspace(4) global [10 x i256] zeroinitializer
@const2 = addrspace(4) global [10 x i256] zeroinitializer

; CHECK-LABEL: consti_loadconst_storeglobal
define void @consti_loadconst_storeglobal() nounwind {
  ; CHECK: add @const[1], r0, stack[@val+1]
  %1 = load i256, ptr addrspace(4) getelementptr inbounds ([10 x i256], ptr addrspace(4) @const, i256 0, i256 1), align 32
  store i256 %1, ptr getelementptr inbounds ([10 x i256], ptr @val, i256 0, i256 1), align 32
  ret void
}

; CHECK-LABEL: consti_loadglobal_storeglobal
define void @consti_loadglobal_storeglobal() nounwind {
  ; CHECK: add stack[@val+7], r0, stack[@val+1]
  %1 = load i256, ptr getelementptr inbounds ([10 x i256], ptr @val, i256 0, i256 7), align 32
  store i256 %1, ptr getelementptr inbounds ([10 x i256], ptr @val, i256 0, i256 1), align 32
  ret void
}

; CHECK-LABEL: vari_loadconst_storeglobal
define void @vari_loadconst_storeglobal(i256 %i) nounwind {
  %addrc = getelementptr inbounds [10 x i256], ptr addrspace(4) @const2, i256 0, i256 %i
  %addrg = getelementptr inbounds [10 x i256], ptr @val, i256 0, i256 %i
  %1 = load i256, ptr addrspace(4) %addrc, align 32
  ; CHECK-NOT: shr.s 5, r1, {{r[0-9]+}}
  ; CHECK: add @const2[r1], r0, stack[@val + r1]
  store i256 %1, ptr %addrg, align 32
  ret void
}

; CHECK-LABEL: vari_loadglobal_storeglobal
define void @vari_loadglobal_storeglobal(i256 %i, i256 %j) nounwind {
  ; CHECK-NOT: shr.s 5, r1, r1
  ; CHECK: add stack[@val + r1], r0, r1
  %addri = getelementptr inbounds [10 x i256], ptr @val, i256 0, i256 %i
  %addrj = getelementptr inbounds [10 x i256], ptr @val, i256 0, i256 %j
  %1 = load i256, ptr %addri, align 32
  ; CHECK-NOT: shr.s 5, r2, r2
  ; CHECK: add r1, r0, stack[@val + r2]
  ; TODO: CPR-1363 Should be folded into a single instruction.
  store i256 %1, ptr %addrj, align 32
  ret void
}

; CHECK-LABEL: stack_register_addressing_storing
define void @stack_register_addressing_storing([10 x i256]* %array, i256 %idx, i256 %val) {
  ; CHECK:  shl.s   5, r[[REG:[0-9]+]], r[[REG]]
  ; CHECK:  add     r1, r[[REG]], r1
  ; CHECK:  shr.s   5, r1, r1
  ; CHECK:  add     r{{[0-9]+}}, r0, stack[r1]
  %idx_slot = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 %idx
  store i256 %val, i256* %idx_slot
  ret void
}

; CHECK-LABEL: alloca_reg_storing
define void @alloca_reg_storing(i256 %idx, i256 %val) {
  ; CHECK:  nop     stack+=[10]
  ; CHECK-NEXT:  context.sp r3
  ; CHECK-NEXT:  add     r3, r1, r1
  ; CHECK-NEXT:  sub.s   10, r1, r1
  ; CHECK-NEXT:  add     r2, r0, stack[r1]
  ; CHECK-NOT:  shl.s   5, r1, r1
  ; CHECK-NOT:  shr.s   5, r1, r1
  ; CHECK-NOT:  add     r2, r0, stack-[10 - r1]
  %array = alloca [10 x i256], align 32
  %idx_slot = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 %idx
  store i256 %val, i256* %idx_slot
  ret void
}

; CHECK-LABEL: alloca_const_storing
define void @alloca_const_storing(i256 %idx, i256 %val) {
  ; CHECK:  nop     stack+=[10]
  ; CHECK:  add     r2, r0, stack-[5]  
  %array = alloca [10 x i256], align 32
  %idx_slot = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 5
  store i256 %val, i256* %idx_slot
  ret void
}

; CHECK-LABEL: stack_register_addressing_loading
define i256 @stack_register_addressing_loading([10 x i256]* %array, i256 %idx) {
  ; CHECK:  shl.s   5, r[[REG2:[0-9]+]], r[[REG2]]
  ; CHECK:  add     r1, r[[REG2]], r1
  ; CHECK:  shr.s   5, r1, r1
  ; CHECK:  add     stack[r1], r0, r1
  %idx_slot = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 %idx
  %rv = load i256, i256* %idx_slot
  ret i256 %rv
}

; CHECK-LABEL: alloca_reg_loading
define i256 @alloca_reg_loading(i256 %idx, i256 %val) {
  ; CHECK:  nop     stack+=[10]
  ; CHECK-NEXT:  context.sp r2
  ; CHECK-NEXT:  add     r2, r1, r1
  ; CHECK-NEXT:  sub.s   10, r1, r1
  ; CHECK-NEXT:  add     stack[r1], r0, r1
  ; CHECK-NOT:  shl.s   5, r1, r1
  ; CHECK-NOT:  shr.s   5, r1, r1
  ; CHECK-NOT:  add     stack-[10 - r1], r0, r1
  %array = alloca [10 x i256], align 32
  %idx_slot = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 %idx
  %rv = load i256, i256* %idx_slot
  ret i256 %rv
}

; CHECK-LABEL: alloca_const_loading
define i256 @alloca_const_loading(i256 %val) {
  ; CHECK:  nop     stack+=[10]
  ; CHECK:  add     stack-[5], r0, r1
  %array = alloca [10 x i256], align 32
  %idx_slot = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 5
  %rv = load i256, i256* %idx_slot
  ret i256 %rv
}

; CHECK-LABEL: arg_array_loading
define i256 @arg_array_loading([10 x i256]* %array) {
  ; CHECK:  shr.s   5, r1, r1
  ; CHECK:  add     stack[5 + r1], r0, r1
  %idx_slot = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 5
  %rv = load i256, i256* %idx_slot
  ret i256 %rv
}

; CHECK-LABEL: arg_array_loading2
define i256 @arg_array_loading2([10 x i256]* %array, i256 %idx) {
  ; CHECK:  shl.s   5, r2, r2
  ; CHECK:  add     r1, r2, r1
  ; CHECK:  shr.s   5, r1, r1
  ; CHECK:  add     stack[r1], r0, r1
  %idx_slot = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 %idx
  %rv = load i256, i256* %idx_slot
  ret i256 %rv
}

; CHECK-LABEL: arg_ptr_loading
define i256 @arg_ptr_loading(i256* %array) {
  ; CHECK:  shr.s   5, r1, r1
  ; CHECK:  add     stack[5 + r1], r0, r1
  %idx_slot = getelementptr i256, i256* %array, i256 5
  %rv = load i256, i256* %idx_slot
  ret i256 %rv
}

; CHECK-LABEL: arg_ptr_loading2
define i256 @arg_ptr_loading2(i256* %array, i256 %idx) {
  ; CHECK:  shl.s   5, r2, r2
  ; CHECK:  add     r1, r2, r1
  ; CHECK:  shr.s   5, r1, r1
  ; CHECK:  add     stack[r1], r0, r1
  %idx_slot = getelementptr i256, i256* %array, i256 %idx
  %rv = load i256, i256* %idx_slot
  ret i256 %rv
}

; CHECK-LABEL: stack_array_passing
define void @stack_array_passing() {
  ; CHECK:  nop     stack+=[10]
  ; CHECK:  context.sp      r[[REG3:[0-9]+]]
  ; CHECK:  sub.s   10, r[[REG3]], r[[REG4:[0-9]+]]
  ; CHECK:  shl.s    5, r[[REG4]], r1
  ; CHECK:  near_call       r0, @array_arg, @DEFAULT_UNWIND
  %array = alloca [10 x i256], align 32
  call void @array_arg([10 x i256]* %array)
  ret void
}

; CHECK-LABEL: stack_pointer_passing
define void @stack_pointer_passing() {
  ; CHECK:  nop     stack+=[10]
  ; CHECK:  context.sp      r[[REG5:[0-9]+]]
  ; CHECK:  sub.s   10, r[[REG5]], r[[REG5]]
  ; CHECK:  shl.s    5, r[[REG5]], r[[REG5]]
  ; CHECK:  add     160, r[[REG5]], r1
  %array = alloca [10 x i256], align 32
  %idx_slot = getelementptr inbounds [10 x i256], [10 x i256]* %array, i256 0, i256 5
  call void @ptr_arg(i256* %idx_slot)
  ret void
}

; CHECK-LABEL: stack_pointer_passing2
define void @stack_pointer_passing2() {
  ; CHECK: nop     stack+=[10]
  ; CHECK: context.sp      r[[REG6:[0-9]+]]
  ; CHECK: sub.s   10, r[[REG6]], r[[REG6]]
  ; CHECK: shl.s    5, r[[REG6]], r1
  %array = alloca [10 x i256], align 32
  call void @array_arg([10 x i256]* %array)
  ret void
}

declare void @ptr_arg(i256*) nounwind
declare void @array_arg([10 x i256]*) nounwind
declare void @pointer_arg(i256*) nounwind

