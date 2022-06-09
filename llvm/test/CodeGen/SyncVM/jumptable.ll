; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:8:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = addrspace(4) global i256 42

; CHECK-LABEL: switch
define i256 @switch(i256 %p1, i256 %p2) nounwind {
entry:
; CHECK: jump code[@JTI0_0 + r1]
  switch i256 %p1, label %default [ i256 0, label %zero
                                    i256 1, label %one
                                    i256 2, label %two
                                    i256 3, label %three
                                    i256 4, label %four ]
default:
  %res = add i256 %p1, %p2
  ret i256 %res
zero:
  ret i256 %p2
one:
  %res.2 = mul i256 %p2, 2
  ret i256 %res.2
two:
  %res.4 = mul i256 %p2, 4
  ret i256 %res.4
three:
  ret i256 %p1
four:
  %res.1 = sub i256 %p1, %p2
  ret i256 %res.1
}

; CHECK: JTI0_0
; CHECK: .cell .BB0_{{[0-9]}}
; CHECK: .cell .BB0_{{[0-9]}}
; CHECK: .cell .BB0_{{[0-9]}}
; CHECK: .cell .BB0_{{[0-9]}}
; CHECK: .cell .BB0_{{[0-9]}}
