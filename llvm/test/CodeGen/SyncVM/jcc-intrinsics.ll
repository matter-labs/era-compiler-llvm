; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:256:256-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: jgt
define i256 @jgt() nounwind {
  call void @foo()
  %1 = call i256 @llvm.syncvm.gtflag()
  %2 = trunc i256 %1 to i1
; CHECK: jgt
  br i1 %2, label %out1, label %out2
out1:
  ret i256 7
out2:
  ret i256 42
}

; CHECK-LABEL: jlt
define i256 @jlt() nounwind {
  call void @foo()
  %1 = call i256 @llvm.syncvm.ltflag()
  %2 = trunc i256 %1 to i1
; CHECK: jlt
  br i1 %2, label %out1, label %out2
out1:
  ret i256 7
out2:
  ret i256 42
}

; CHECK-LABEL: je
define i256 @je() nounwind {
  call void @foo()
  %1 = call i256 @llvm.syncvm.eqflag()
  %2 = trunc i256 %1 to i1
; CHECK: je
  br i1 %2, label %out1, label %out2
out1:
  ret i256 7
out2:
  ret i256 42
}

declare i256 @llvm.syncvm.gtflag()
declare i256 @llvm.syncvm.ltflag()
declare i256 @llvm.syncvm.eqflag()
declare void @foo()
