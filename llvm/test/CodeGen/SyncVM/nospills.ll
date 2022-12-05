; RUN: llc -syncvm-no-combine-spills-when-frame-empty=true < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"


; CHECK: simple_conversion
define i256 @simple_conversion(i256 %a, i256 %b, i256 %c, i256 %d) nounwind {
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}, r{{[0-9]+}}
  %r1 = add i256 %a, %b
  %r2 = add i256 %c, %d
  %r3 = add i256 %r1, %r2
  ret i256 %r3
}

