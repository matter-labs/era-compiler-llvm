; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; test: do not convert: -(X >>u 31) -> (X >>s 31)
; CHECK-LABEL: neg_shift_right
define i256 @neg_shift_right(i256 %X) {
entry:
  %0 = lshr i256 %X, 255; Logical shift right by BitWidth - 1
  %1 = sub i256 0, %0   ; Negate the result
; CHECK: shr.s   255, r1, r1
; CHECK: sub     0, r1, r1
  ret i256 %1           ; Return the result
}

; test: do not convert: sub N0, (lshr N10, width-1) --> add N0, (ashr N10, width-1)
; CHECK-LABEL: sub_lshr_width_minus_1
define i256 @sub_lshr_width_minus_1(i256 %N0, i256 %N10) {
entry:
  %0 = lshr i256 %N10, 255; Logical shift right by 255
  %1 = sub i256 %N0, %0   ; Subtract the shifted value from N0
; CHECK: shr.s   255, r2, r2
; CHECK: sub     r1, r2, r1
  ret i256 %1             ; Return the result
}

; test: do not convert (urem x, (shl pow2, y)) -> (and x, (add (shl pow2, y), -1))
; CHECK-LABEL: urem_shl
define i256 @urem_shl(i256 %x, i256 %y) {
entry:
  %0 = shl i256 128, %y  ; Shift left pow2 by y bits
  %1 = urem i256 %x, %0     ; Unsigned remainder of x divided by the shifted value
; CHECK: shl     128, r2, r2
; CHECK: div     r1, r2, r2, r1
  ret i256 %1               ; Return the result
}

; test: do not convert: sext (not i1 X) -> add (sra X, 31), (C + 1)
; CHECK-LABEL: sext_not
define i256 @sext_not(i1 %X) {
entry:
  %0 = xor i1 %X, true ; Logical negation (NOT) of X
  %1 = sext i1 %0 to i256 ; Sign extend the result
; CHECK: sub.s   1, r1, r1
  ret i256 %1 ; Return the result
}

; test: do not convert: add (srl (not X), 31), C -> add (sra X, 31), (C + 1)
define i256 @add_srl_not_constant(i256 %X) {
entry:
  %0 = xor i256 %X, -1    ; Logical negation (NOT) of X
  %1 = lshr i256 %0, 31   ; Logical shift right by 31 bits
  %2 = add i256 %1, 42    ; Add the constant C
; CHECK: sub.s   1, r0, r2
; CHECK: xor     r1, r2, r1
; CHECK: shr.s   31, r1, r1
; CHECK: add     42, r1, r1
; CHECK: ret
  ret i256 %2             ; Return the result
}


; test: do not convert: (and (srl i256:x, K), KMask) ->
;       (i64 zero_extend (and (srl (i256 (trunc i256:x)), K)), KMask)
; CHECK-LABEL: bit_extract
define i256 @bit_extract(i256 %x) {
entry:
  %0 = lshr i256 %x, 128 ; Logical shift right by K (example value: 128)
  %1 = and i256 %0, 340282366920938463463374607431768211455 ; Bitwise AND with KMask
; CHECK: shr.s   128, r1, r1
  ret i256 %1 ; Return the result
}

