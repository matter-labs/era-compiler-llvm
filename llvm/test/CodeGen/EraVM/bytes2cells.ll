; RUN: llc < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

; check that when base pointer is a stack address (passed down as argument in cells),
; it is scaled for pointer arithmetics.
; because the index offset is unknown so optimizing this is skipped
; CHECK-LABEL: return_elem_var
define ptr @return_elem_var(ptr %array, i256 %i) {
; CHECK: shl.s 5, r2, r2
; CHECK: add   r1, r2, r[[REG:[0-9]+]]
; CHECK: shr.s 5, r[[REG]], r[[REG]]
  %addri = getelementptr inbounds [10 x i256], ptr %array, i256 0, i256 %i
  store i256 0, ptr %addri
  ret ptr %array
}

; pointer arithmetic result as return value
; CHECK-LABEL: return_elem_var_2
define ptr @return_elem_var_2(i256* %array) nounwind {
  %addri = getelementptr inbounds [10 x i256], ptr %array, i256 0, i256 7
  store i256 0, ptr %addri
; CHECK: shr.s 5, r1, r[[REG:[0-9]+]]
; CHECK: add   0, r0, stack[7 + r[[REG]]]
; CHECK: add   224, r1, r1
; CHECK: ret
  ret ptr %addri
}

; return stack pointer. return cell pointer as is
; CHECK-LABEL: return_ptr
define ptr @return_ptr(i256* %array) nounwind {
  ; CHECK-NOT: shl.s
  ret ptr %array
}

declare void @foo(ptr %val);

; passing stack pointer in cells as argument, without unnecessary scaling
; CHECK-LABEL: call_foo
define void @call_foo() nounwind {
; CHECK: sp [[REG:r[0-9]+]]
; CHECK: sub.s 1, [[REG]], [[REG2:r[0-9]+]]
; CHECK-NOT: shr.s
; CHECK: call
  %p = alloca i256
  call void @foo(ptr %p)
  ret void
}

declare ptr @bar();

; passing stack pointer in cells as argument
; CHECK-LABEL: call_bar
define ptr @call_bar() nounwind {
; CHECK-NOT: shl.s
; CHECK-NOT: shr.s
  %p = call ptr @bar();
  ret ptr %p
}

; passing stack pointer in cells as argument
; CHECK-LABEL: call_bar2
define ptr @call_bar2() nounwind {
  %p = call ptr @bar();
  %p2 = getelementptr inbounds [10 x i256], ptr %p, i256 0, i256 7
; CHECK-NOT: shr.s 5, [[REG_BAR:r[0-9]+]], r1
  store i256 0, ptr %p2
  ret ptr %p2
}

; passing stack pointer
; CHECK-LABEL: call_bar3
define ptr @call_bar3() nounwind {
; CHECK:     add 224, r1, r1
; CHECK-NOT: shr.s 5, r1, r1
; CHECK: ret
  %p = call ptr @bar();
  %p2 = getelementptr inbounds [10 x i256], ptr %p, i256 0, i256 7
  ret ptr %p2
}
