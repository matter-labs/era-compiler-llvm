; RUN: llc -opaque-pointers < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare { i256, i1 } @llvm.uadd.with.overflow.i256(i256, i256)
declare { i256, i1 } @llvm.usub.with.overflow.i256(i256, i256)
declare { i256, i1 } @llvm.umul.with.overflow.i256(i256, i256)
declare void @has_overflow()
declare void @has_no_overflow()

; CHECK-LABEL: uaddo_i256
define i256 @uaddo_i256(i256 %a, i256 %b, i1* %overflow) {
entry:
  %result = call { i256, i1 } @llvm.uadd.with.overflow.i256(i256 %a, i256 %b)
  %sum = extractvalue { i256, i1 } %result, 0
  %flag = extractvalue { i256, i1 } %result, 1
  store i1 %flag, i1* %overflow
  ret i256 %sum
}


; CHECK-LABEL: umulo_i256
define i256 @umulo_i256(i256 %a, i256 %b, i1* %overflow) {
entry:
  %result = call { i256, i1 } @llvm.umul.with.overflow.i256(i256 %a, i256 %b)
  %product = extractvalue { i256, i1 } %result, 0
  %flag = extractvalue { i256, i1 } %result, 1
  store i1 %flag, i1* %overflow
  ret i256 %product
}


; CHECK-LABEL: usubo_i256
define i256 @usubo_i256(i256 %a, i256 %b, i1* %overflow) {
entry:
  %result = call { i256, i1 } @llvm.usub.with.overflow.i256(i256 %a, i256 %b)
  %difference = extractvalue { i256, i1 } %result, 0
  %flag = extractvalue { i256, i1 } %result, 1
  store i1 %flag, i1* %overflow
  ret i256 %difference
}


; CHECK-LABEL: uadd_branch_1
define void @uadd_branch_1(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 %y)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! r1, r2, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_2
define void @uadd_branch_2(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 %y)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected
; CHECK: add! r1, r2, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

; only the branches layout is different from previous function
; and we should check that it still generates the same code
no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

overflow_detected:
  call void @has_overflow()
  br label %exit

exit:
  ret void 
}

; CHECK-LABEL: uadd_branch_reg_reg
define i256 @uadd_branch_reg_reg(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 %y)
  %sum = extractvalue {i256, i1} %res1, 0
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! r{{[0-9]+}}, r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret i256 %sum
}

; CHECK-LABEL: uadd_branch_imm_reg
define void @uadd_branch_imm_reg(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 1024)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! 1024, r1, r1
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_code_reg
define void @uadd_branch_code_reg(i256 %x, i256 addrspace(4)* %y_ptr) {
entry:
  %y = load i256, i256 addrspace(4)* %y_ptr
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 %y)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! code[r2], r1, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND
overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_code_reg_2
define void @uadd_branch_code_reg_2(i256 %x, i256 addrspace(4)* %y_ptr) {
entry:
  %y = load i256, i256 addrspace(4)* %y_ptr
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %y, i256 %x)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! code[r2], r1, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND
overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_stack_reg
define void @uadd_branch_stack_reg(i256 %x, i256* %y_ptr) {
entry:
  %y = load i256, i256* %y_ptr
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 %y)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! stack[r2], r1, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND
overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_stack_reg_2
define void @uadd_branch_stack_reg_2(i256 %x, i256* %y_ptr) {
entry:
  %y = load i256, i256* %y_ptr
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %y, i256 %x)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! stack[r2], r1, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND
overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_code_stack
define void @uadd_branch_code_stack(i256 %x, i256 addrspace(4)* %valptr) nounwind {
  %destptr = alloca i256

  %val = load i256, i256 addrspace(4)* %valptr
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %val, i256 %x)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: shr.s 5, r[[REG:[0-9]+]], r[[REG]]
; CHECK: add! code[r[[REG]]], r1, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_code_stack_2
define void @uadd_branch_code_stack_2(i256 %x, i256 addrspace(4)* %valptr) nounwind {
  %destptr = alloca i256

  %val = load i256, i256 addrspace(4)* %valptr
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 %val)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: shr.s 5, r[[REG:[0-9]+]], r[[REG]]
; CHECK: add! code[r[[REG]]], r1, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_stack_stack
define void @uadd_branch_stack_stack(i256 %x) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256

  %val = load i256, i256* %valptr
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %val, i256 %x)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! stack-[2], r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_stack_stack_2
define void @uadd_branch_stack_stack_2(i256 %x) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256

  %val = load i256, i256* %valptr
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %val, i256 %x)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! stack-[2], r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_imm_stack
define void @uadd_branch_imm_stack(i256 %rs1) nounwind {
  %destptr = alloca i256

  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 1024, i256 %rs1)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! 1024, r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: uadd_branch_imm_stack_2
define void @uadd_branch_imm_stack_2(i256 %rs1) nounwind {
  %destptr = alloca i256

  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %rs1, i256 1024)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! 1024, r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_1
define void @usub_branch_1(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %x, i256 %y)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected
; CHECK: sub!
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_2
define void @usub_branch_2(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %x, i256 %y)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected
; CHECK: sub!
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

; only the branches layout is different from previous function
; and we should check that it still generates the same code
no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

overflow_detected:
  call void @has_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_3
define i256 @usub_branch_3(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %x, i256 %y)
  %sum = extractvalue {i256, i1} %res1, 0
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub! r{{[0-9]+}}, r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret i256 %sum
}

; CHECK-LABEL: usub_branch_imm_reg
define void @usub_branch_imm_reg(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %x, i256 1024)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub.s!  1024, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_imm_reg_2
define void @usub_branch_imm_reg_2(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 1024, i256 %x)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub!  1024, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_code_reg
define void @usub_branch_code_reg(i256 %x, i256 addrspace(4)* %y_ptr) {
entry:
  %y = load i256, i256 addrspace(4)* %y_ptr
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %x, i256 %y)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub.s! code[r{{[0-9]+}}], r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_code_reg_2
define void @usub_branch_code_reg_2(i256 %x, i256 addrspace(4)* %y_ptr) {
entry:
  %y = load i256, i256 addrspace(4)* %y_ptr
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %y, i256 %x)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub! code[r2], r1, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_stack_reg
define void @usub_branch_stack_reg(i256 %inc) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256

  %val = load i256, i256* %valptr
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %val, i256 %inc)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub! stack-[2], r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_stack_reg_2
define void @usub_branch_stack_reg_2(i256 %inc) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256

  %val = load i256, i256* %valptr
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %inc, i256 %val)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub.s! stack-[2], r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_imm_stack
define void @usub_branch_imm_stack(i256 %rs1) nounwind {
  %destptr = alloca i256

  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 1024, i256 %rs1)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub! 1024, r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_imm_stack_2
define void @usub_branch_imm_stack_2(i256 %rs1) nounwind {
  %destptr = alloca i256

  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %rs1, i256 1024)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub.s! 1024, r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_code_stack
define void @usub_branch_code_stack(i256 %inc, i256 addrspace(4)* %valptr) nounwind {
  %destptr = alloca i256

  %val = load i256, i256 addrspace(4)* %valptr
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %val, i256 %inc)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub! code[r2], r1, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND


overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_code_stack_2
define void @usub_branch_code_stack_2(i256 %inc, i256 addrspace(4)* %valptr) nounwind {
  %destptr = alloca i256

  %val = load i256, i256 addrspace(4)* %valptr
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %inc, i256 %val)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub.s! code[r{{[0-9]+}}], r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND


overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_stack_stack
define void @usub_branch_stack_stack(i256 %inc) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256

  %val = load i256, i256* %valptr
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %val, i256 %inc)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub! stack-[2], r{{[0-9]+}}, stack-[1]
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND


overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: usub_branch_stack_stack_2
define void @usub_branch_stack_stack_2(i256 %inc) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256

  %val = load i256, i256* %valptr
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %inc, i256 %val)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: sub.s! stack-[2], r{{[0-9]+}}, stack-[1]
; CHECK: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND


overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_1
define void @umul_branch_1(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %x, i256 %y)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_2
define void @umul_branch_2(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %x, i256 %y)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

; only the branches layout is different from previous function
; and we should check that it still generates the same code
no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

overflow_detected:
  call void @has_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_reg_reg
define i256 @umul_branch_reg_reg(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %x, i256 %y)
  %sum = extractvalue {i256, i1} %res1, 0
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! r{{[0-9]+}}, r{{[0-9]+}}, stack-[1], r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret i256 %sum
}

; CHECK-LABEL: umul_branch_imm_reg
define void @umul_branch_imm_reg(i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %x, i256 1024)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! 1024, r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_code_reg
define void @umul_branch_code_reg(i256 %x, i256 addrspace(4)* %y_ptr) {
entry:
  %y = load i256, i256 addrspace(4)* %y_ptr
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %x, i256 %y)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! code[r2], r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_code_reg_2
define void @umul_branch_code_reg_2(i256 %x, i256 addrspace(4)* %y_ptr) {
entry:
  %y = load i256, i256 addrspace(4)* %y_ptr
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %y, i256 %x)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! code[r2], r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_stack_reg
define void @umul_branch_stack_reg(i256 %x, i256* %y_ptr) {
entry:
  %y = load i256, i256* %y_ptr
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %y, i256 %x)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: add! stack[r2], r1, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND
overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_stack_reg_2
define void @umul_branch_stack_reg_2(i256 %x, i256* %y_ptr) {
entry:
  %y = load i256, i256* %y_ptr
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %y, i256 %x)
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! stack[r2], r1, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND
overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_stack_stack
define void @umul_branch_stack_stack(i256 %x) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256

  %val = load i256, i256* %valptr
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %val, i256 %x)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! stack-[2], r{{[0-9]+}}, stack-[1], r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND


overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_stack_stack_2
define void @umul_branch_stack_stack_2(i256 %x) nounwind {
  %valptr = alloca i256
  %destptr = alloca i256

  %val = load i256, i256* %valptr
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %x, i256 %val)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! stack-[2], r{{[0-9]+}}, stack-[1], r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND


overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_imm_stack
define void @umul_branch_imm_stack(i256 %rs1) nounwind {
  %destptr = alloca i256

  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 1024, i256 %rs1)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! 1024, r{{[0-9]+}}, stack-[1], r{{[0-9]+}}
; CHECK: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_imm_stack_2
define void @umul_branch_imm_stack_2(i256 %rs1) nounwind {
  %destptr = alloca i256

  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %rs1, i256 1024)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! 1024, r{{[0-9]+}}, stack-[1], r{{[0-9]+}}
; CHECK: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND

overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_code_stack
define void @umul_branch_code_stack(i256 %inc, i256 addrspace(4)* %valptr) nounwind {
  %destptr = alloca i256

  %val = load i256, i256 addrspace(4)* %valptr
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %val, i256 %inc)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! code[r2], r1, stack-[1], r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND


overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: umul_branch_code_stack_2
define void @umul_branch_code_stack_2(i256 %inc, i256 addrspace(4)* %valptr) nounwind {
  %destptr = alloca i256

  %val = load i256, i256 addrspace(4)* %valptr
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %inc, i256 %val)
  %sum = extractvalue {i256, i1} %res1, 0
  store i256 %sum, i256* %destptr
  %overflow = extractvalue {i256, i1} %res1, 1
  br i1 %overflow, label %overflow_detected, label %no_overflow_detected

; CHECK: mul! code[r2], r1, stack-[1], r{{[0-9]+}}
; CHECK-NEXT: jump.of
; CHECK-NEXT: ; %bb.{{[0-9]+}}:
; CHECK-NEXT: near_call       r0, @has_no_overflow, @DEFAULT_UNWIND


overflow_detected:
  call void @has_overflow()
  br label %exit

no_overflow_detected:
  call void @has_no_overflow()
  br label %exit

exit:
  ret void
}

@constant = global i256 13479381560982165986356396 ; constant
; CHECK-LABEL: modulo_sub
define i256 @modulo_sub(i256 %a_val, i256 %b_val) {
entry:
  ; Compute a + b and store it in c
  %c_val = add i256 %a_val, %b_val
  
  ; Compare c with a
  %cmp = icmp slt i256 %c_val, %a_val
  
  ; Branch based on the comparison result
  br i1 %cmp, label %true_branch, label %exit
  
true_branch:
  ; Load constant
  %constant_val = load i256, i256* @constant
  
  ; Subtract constant from c
  %new_c_val = sub i256 %c_val, %constant_val
  br label %exit

exit:
  %rv = phi i256 [ %c_val, %entry ], [ %new_c_val, %true_branch ]
  ret i256 %rv
}



; CHECK-LABEL: add_test
define i256 @add_test(i256 %a, i256 %b, i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %x, i256 %y)
; CHECK:      add!    r3, r4, r{{[0-9]+}}
; add.ge is add.of reversed
; CHECK-NEXT: add.ge  r2, r0, r1
  %overflow = extractvalue {i256, i1} %res1, 1
  %selected = select i1 %overflow, i256 %a, i256 %b
  ret i256 %selected
}

; CHECK-LABEL: add_test_2
define void @add_test_2(i256 %a, i256 %b, i256 %c, i256 %d) {
  %resptr = alloca i256
  %res1 = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %c, i256 %d)
  %overflow = extractvalue {i256, i1} %res1, 1
  %val = select i1 %overflow, i256 42, i256 %a
; CHECK: add!    r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add.of  42, r0, r1
  store i256 %val, i256* %resptr
  ret void
}

; CHECK-LABEL: sub_test
define i256 @sub_test(i256 %a, i256 %b, i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %x, i256 %y)
; CHECK:      sub!    r3, r4, r{{[0-9]+}}
; add.ge is add.of reversed
; CHECK-NEXT: add.ge  r2, r0, r1
  %overflow = extractvalue {i256, i1} %res1, 1
  %selected = select i1 %overflow, i256 %a, i256 %b
  ret i256 %selected
}

; CHECK-LABEL: sub_test_2
define void @sub_test_2(i256 %a, i256 %b, i256 %c, i256 %d) {
  %resptr = alloca i256
  %res1 = call {i256, i1} @llvm.usub.with.overflow.i256(i256 %c, i256 %d)
  %overflow = extractvalue {i256, i1} %res1, 1
  %val = select i1 %overflow, i256 42, i256 %a
; CHECK: sub!    r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add.of  42, r0, r1
  store i256 %val, i256* %resptr
  ret void
}

; CHECK-LABEL: mul_test
define i256 @mul_test(i256 %a, i256 %b, i256 %x, i256 %y) {
entry:
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %x, i256 %y)
; CHECK:      mul!    r3, r4, r{{[0-9]+}}, r{{[0-9]+}}
; add.ge is add.of reversed
; CHECK-NEXT: add.ge  r2, r0, r1
  %overflow = extractvalue {i256, i1} %res1, 1
  %selected = select i1 %overflow, i256 %a, i256 %b
  ret i256 %selected
}

; CHECK-LABEL: mul_test_2
define void @mul_test_2(i256 %a, i256 %b, i256 %c, i256 %d) {
  %resptr = alloca i256
  %res1 = call {i256, i1} @llvm.umul.with.overflow.i256(i256 %c, i256 %d)
  %overflow = extractvalue {i256, i1} %res1, 1
  %val = select i1 %overflow, i256 42, i256 %a
; CHECK: mul!    r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add.of  42, r0, r1
  store i256 %val, i256* %resptr
  ret void
}
