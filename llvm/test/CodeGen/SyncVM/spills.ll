; RUN: llc -syncvm-no-combine-spills-when-frame-empty=false < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"


; CHECK: simple_conversion
define i256 @simple_conversion(i256 %a, i256 %b, i256 %c, i256 %d) nounwind {
; CHECK: nop stack+=[1]
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}, stack-[1]
; CHECK: sub.s stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %r1 = add i256 %a, %b
  %r2 = add i256 %c, %d
  %r3 = sub i256 %r1, %r2
  ret i256 %r3
}

; CHECK: simple_conversion2
define i256 @simple_conversion2(i256 %a, i256 %b) nounwind {
  %r1 = add i256 %a, 123
; CHECK: add 123, r1, stack-[1]
; CHECK: xor stack-[1], r2, r1
  %r2 = xor i256 %b, %r1
  ret i256 %r2
}

; CHECK: simple_conversion3
define i256 @simple_conversion3(i256 %a, i256 %b) nounwind {
  %r1 = add i256 %a, %b
; CHECK: add stack-[1], r2, r{{[0-9]+}}
  %r2 = add i256 %r1, %b
  ret i256 %r2
}

; CHECK: multiple_conversion
define i256 @multiple_conversion(i256 %a, i256 %b, i256 %c) nounwind {
  %r1 = add i256 %a, 123
  %r2 = add i256 %b, 456
; CHECK: xor stack-[1], r1, r1
  %r3 = xor i256 %r2, %r1
  ret i256 %r3
}

; CHECK: multiple_conversion2
define i256 @multiple_conversion2(i256 %a, i256 %b, i256 %c, i256 %d) nounwind {
  %r1 = add i256 %a, 123
  %r2 = add i256 %b, 456
; CHECK: xor stack-[{{[0-9]+}}], r{{[0-9]+}}, stack-[{{[0-9]+}}]
; CHECK: xor stack-[{{[0-9]+}}], r{{[0-9]+}}, stack-[{{[0-9]+}}]
; CHECK: and stack-[{{[0-9]+}}], r{{[0-9]+}}, r{{[0-9]+}}
  %r3 = xor i256 %r2, %r1
  %r4 = xor i256 %r3, %c
  %r5 = and i256 %r4, %d
  ret i256 %r5
}

; CHECK: second_operand_conversion
define i256 @second_operand_conversion(i256 %a, i256 %b, i256 %c) nounwind {
; TODO: xor 123, r{{[0-9]+}}, stack-[1]
; TODO: add stack-[1], r{{[0-9]+}}, r1
  %r1 = xor i256 %a, 123
  %r2 = add i256 %c, %r1
  ret i256 %r2
}

; CHECK: second_operand_conversion2
define i256 @second_operand_conversion2(i256 %a, i256 %b, i256 %c) nounwind {
; TODO: xor r1, r2, stack-[1]
; TODO: add stack-[1], r{{[0-9]+}}, r1
  %r1 = xor i256 %a, %b
  %r2 = add i256 %c, %r1
  ret i256 %r2
}

; CHECK: second_opnd_conversion
define i256 @second_opnd_conversion(i256 %a, i256 %b) nounwind {
; CHECK: xor 123, r{{[0-9]+}}, stack-[1]
  %r1 = xor i256 %a, 123
; CHECK: sub.s stack-[1], r{{[0-9]+}}, r1
  %r3 = sub i256 %b, %r1
  %r4 = call i256 @conversion_test(i256 %r3)
  ret i256 %r4
}

; CHECK: immediate_conversion
define i256 @immediate_conversion(i256 %a, i256 %b, i256 %c) nounwind {
; CHECK: add 123, r{{[0-9]+}}, stack-[1]
  %r1 = add i256 %a, 123
  %r2 = add i256 %b, 456
; CHECK: xor stack-[1], r{{[0-9]+}}, r{{[0-9]+}}
  %r3 = xor i256 %r2, %r1
  %r4 = call i256 @conversion_test(i256 %r3)
  ret i256 %r4
}

declare i256 @conversion_test(i256 %a) nounwind
