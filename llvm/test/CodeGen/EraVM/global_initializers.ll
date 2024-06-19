; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@glob = global i256 113
@glob.arr = global [4 x i256] [i256 1, i256 29, i256 37, i256 4]
@glob_ptr_as3 = global i256* zeroinitializer
@glob.arr.as4 = addrspace(4) global [4 x i256] zeroinitializer
@glob.const = constant i256 737

; CHECK-LABEL: .text
; CHECK-NEXT: nop	stack+=[10 + r0]
; CHECK-NEXT: add	@glob_initializer[0], r0, stack[@glob]
; CHECK-NEXT: add	@glob.arr_initializer[0], r0, stack[@glob.arr]
; CHECK-NEXT: add	@glob.arr_initializer[1], r0, stack[@glob.arr + 1]
; CHECK-NEXT: add	@glob.arr_initializer[2], r0, stack[@glob.arr + 2]
; CHECK-NEXT: add	@glob.arr_initializer[3], r0, stack[@glob.arr + 3]


; CHECK-LABEL: glob_initializer:
; CHECK-NEXT:  .cell	113

; CHECK-LABEL: glob.arr_initializer:
; CHECK-NEXT:  .cell	1
; CHECK-NEXT:  .cell	29
; CHECK-NEXT:  .cell	37
; CHECK-NEXT:  .cell	4

; CHECK-LABEL: glob_ptr_as3_initializer:
; CHECK-NEXT:  .cell	0

; CHECK-LABEL: glob.arr.as4_initializer:
; CHECK-NEXT:  .zero	128

; CHECK-NOT: glob.const_initializer:

; CHECK-LABEL: DEFAULT_UNWIND:
; CHECK-NEXT:  ret.panic.to_label	@DEFAULT_UNWIND

; CHECK-LABEL: DEFAULT_FAR_RETURN:
; CHECK-LABEL: ret.ok.to_label	r1, @DEFAULT_FAR_RETURN

; CHECK-LABEL: DEFAULT_FAR_REVERT:
; CHECK-LABEL: ret.revert.to_label	r1, @DEFAULT_FAR_REVERT

define i256 @get_glob() nounwind {
  %res = load i256, i256* @glob
  ret i256 %res
}
