; RUN: llc -O3 < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @no_sink(i1 %cond) {
; CHECK-LABEL: no_sink
; CHECK:        sub! r1, r0, r1
; CHECK-NEXT:   jump.eq @.BB0_2
; CHECK:        add 2, r0, r1
; CHECK-NEXT:   st.2 3, r1
; CHECK-NEXT:   jump @.BB0_3
; CHECK:       .BB0_2:
; CHECK:        add 2, r0, r1
; CHECK-NEXT:   st.1 2, r1
; CHECK:       .BB0_3:
; CHECK:        context.gas_left r1
; CHECK:        ret
;
entry:
  br i1 %cond, label %then, label %else

then:
  store i256 2, ptr addrspace(2) inttoptr (i256 3 to ptr addrspace(2)), align 64
  %val1 = call i256 @llvm.eravm.gasleft()
  br label %ret

else:
  store i256 2, ptr addrspace(1) inttoptr (i256 2 to ptr addrspace(1)), align 64
  %val2 = call i256 @llvm.eravm.gasleft()
  br label %ret

ret:
  %phi = phi i256 [ %val1, %then ], [ %val2, %else ]
  ret i256 %phi
}

declare i256 @llvm.eravm.gasleft()
