; RUN: opt -O3 -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @no_hoist(i1 %cond) {
; CHECK-LABEL: @no_hoist(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br i1 [[COND:%.*]], label [[THEN:%.*]], label [[ELSE:%.*]]
; CHECK:       then:
; CHECK-NEXT:    [[VAL1:%.*]] = tail call i256 @llvm.eravm.gasleft()
; CHECK-NEXT:    store i256 2, ptr addrspace(2) inttoptr (i256 3 to ptr addrspace(2)), align 64
; CHECK-NEXT:    [[RES1:%.*]] = add i256 [[VAL1]], 5
; CHECK-NEXT:    br label [[RET:%.*]]
; CHECK:       else:
; CHECK-NEXT:    [[VAL2:%.*]] = tail call i256 @llvm.eravm.gasleft()
; CHECK-NEXT:    store i256 2, ptr addrspace(1) inttoptr (i256 2 to ptr addrspace(1)), align 64
; CHECK-NEXT:    [[RES2:%.*]] = add i256 [[VAL2]], 6
; CHECK-NEXT:    br label [[RET]]
; CHECK:       ret:
; CHECK-NEXT:    [[PHI:%.*]] = phi i256 [ [[RES1]], [[THEN]] ], [ [[RES2]], [[ELSE]] ]
; CHECK-NEXT:    ret i256 [[PHI]]
;
entry:
  br i1 %cond, label %then, label %else

then:
  %val1 = call i256 @llvm.eravm.gasleft()
  store i256 2, ptr addrspace(2) inttoptr (i256 3 to ptr addrspace(2)), align 64
  %res1 = add i256 %val1, 5
  br label %ret

else:
  %val2 = call i256 @llvm.eravm.gasleft()
  store i256 2, ptr addrspace(1) inttoptr (i256 2 to ptr addrspace(1)), align 64
  %res2 = add i256 %val2, 6
  br label %ret

ret:
  %phi = phi i256 [ %res1, %then ], [ %res2, %else ]
  ret i256 %phi
}

define i256 @no_sink(i1 %cond) {
; CHECK-LABEL: @no_sink(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br i1 [[COND:%.*]], label [[THEN:%.*]], label [[ELSE:%.*]]
; CHECK:       then:
; CHECK-NEXT:    store i256 2, ptr addrspace(2) inttoptr (i256 3 to ptr addrspace(2)), align 64
; CHECK-NEXT:    [[VAL1:%.*]] = tail call i256 @llvm.eravm.gasleft()
; CHECK-NEXT:    br label [[RET:%.*]]
; CHECK:       else:
; CHECK-NEXT:    store i256 2, ptr addrspace(1) inttoptr (i256 2 to ptr addrspace(1)), align 64
; CHECK-NEXT:    [[VAL2:%.*]] = tail call i256 @llvm.eravm.gasleft()
; CHECK-NEXT:    br label [[RET]]
; CHECK:       ret:
; CHECK-NEXT:    [[PHI:%.*]] = phi i256 [ [[VAL1]], [[THEN]] ], [ [[VAL2]], [[ELSE]] ]
; CHECK-NEXT:    ret i256 [[PHI]]
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
