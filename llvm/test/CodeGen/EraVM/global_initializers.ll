; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

@glob = global i256 113
@glob.arr = global [4 x i256] [i256 0, i256 29, i256 0, i256 4]
@glob_ptr_as3 = global i256* zeroinitializer
@glob.arr.as4 = addrspace(4) global [4 x i256] zeroinitializer
@glob.const = constant i256 737

; CHECK-LABEL: .text
; CHECK-NEXT: incsp 6
; CHECK-NEXT: add	code[@glob_initializer_0], r0, stack[@glob]
; CHECK-NEXT: add	code[@glob.arr_initializer_1], r0, stack[@glob.arr + 1]
; CHECK-NEXT: add	code[@glob.arr_initializer_3], r0, stack[@glob.arr + 3]


; CHECK-LABEL: glob_initializer_0:
; CHECK-NEXT:  .cell	113

; CHECK-NOT: glob.const_initializer_0:
; CHECK-LABEL: glob.arr_initializer_1:
; CHECK-NEXT:  .cell	29
; CHECK-NOT: glob.const_initializer_2:
; CHECK-LABEL: glob.arr_initializer_3:
; CHECK-NEXT:  .cell	4

; CHECK-NOT: glob_ptr_as3_initializer:

; CHECK-NOT: glob.arr.as4_initializer:

; CHECK-NOT: glob.const_initializer:

; CHECK-LABEL: DEFAULT_UNWIND:
; CHECK-NEXT:  pncl @DEFAULT_UNWIND

; CHECK-LABEL: DEFAULT_FAR_RETURN:
; CHECK-LABEL: retl @DEFAULT_FAR_RETURN

; CHECK-LABEL: DEFAULT_FAR_REVERT:
; CHECK-LABEL: revl @DEFAULT_FAR_REVERT

define i256 @get_glob() nounwind {
  %res = load i256, i256* @glob
  ret i256 %res
}
