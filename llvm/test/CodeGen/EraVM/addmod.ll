; RUN: opt -O3 < %s | llc -O3 | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @test(i256 %a) {
; CHECK-LABEL: test
; CHECK:       sub.s 5, r0, r3
; CHECK-NEXT:  sub! r1, r3, r2
; CHECK-NEXT:  add.ge 5, r1, r1
; CHECK-NEXT:  shl.s 1, r1, r4
; CHECK-NEXT:  add 5, r4, r2
; CHECK-NEXT:  sub! r4, r3, r3
; CHECK-NEXT:  add r2, r0, r3
; CHECK-NEXT:  add.lt r4, r0, r3
; CHECK-NEXT:  sub! r4, r1, r1
; CHECK-NEXT:  add.ge r3, r0, r2
; CHECK-NEXT:  add r2, r0, r1
; CHECK-NEXT:  ret
entry:
  %res = call i256 @__addmod(i256 %a, i256 %a, i256 -5)
  ret i256 %res
}

declare i256 @__addmod(i256, i256, i256) #0
attributes #7 = { mustprogress nofree nounwind null_pointer_is_valid readnone willreturn }
