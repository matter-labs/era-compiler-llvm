; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; CHECK-LABEL: srac
define i256 @srac(i256 %a) nounwind {
; CHECK-DAG:   shr
; CHECK-DAG:   shl
; CHECK:   or
; CHECK:   ret
  %1 = ashr i256 %a, 42
  ret i256 %1
}

; CHECK-LABEL: srar
define i256 @srar(i256 %a, i256 %size) nounwind {
; CHECK-DAG:   shr
; CHECK-DAG:   shl
; CHECK:   or
; CHECK:   ret
  %1 = ashr i256 %a, %size
  ret i256 %1
}

; CHECK-LABEL: csra
define i256 @csra(i256 %size) nounwind {
; CHECK: sfll	#42, r2, r2
; CHECK: sflh	#0, r2, r2
; CHECK: shr	r2, r1, r1
; CHECK: ret
  %1 = ashr i256 42, %size
  ret i256 %1
}

; CHECK-LABEL: divr
define i256 @divr(i256 %a, i256 %b) nounwind {
  %1 = sdiv i256 %a, %b
  ret i256 %1
}

; CHECK-LABEL: remr
define i256 @remr(i256 %a, i256 %b) nounwind {
  %1 = srem i256 %a, %b
  ret i256 %1
}

; CHECK-LABEL: sgtr
define i256 @sgtr(i256 %a, i256 %b) nounwind {
  %1 = icmp sgt i256 %a, %b
  %2 = select i1 %1, i256 42, i256 -1
  ret i256 %2
}

; CHECK-LABEL: sltr
define i256 @sltr(i256 %a, i256 %b) nounwind {
  %1 = icmp slt i256 %a, %b
  %2 = select i1 %1, i256 42, i256 -1
  ret i256 %2
}

; CHECK-LABEL: sger
define i256 @sger(i256 %a, i256 %b) nounwind {
  %1 = icmp sge i256 %a, %b
  %2 = select i1 %1, i256 42, i256 -1
  ret i256 %2
}

; CHECK-LABEL: sler
define i256 @sler(i256 %a, i256 %b) nounwind {
  %1 = icmp sle i256 %a, %b
  %2 = select i1 %1, i256 42, i256 -1
  ret i256 %2
}
