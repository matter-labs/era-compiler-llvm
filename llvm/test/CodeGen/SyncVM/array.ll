; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

@val = global [10 x i256] zeroinitializer
@const = addrspace(4) global [10 x i256] zeroinitializer

; CHECK-LABEL: consti_loadconst_storeglobal
define void @consti_loadconst_storeglobal() nounwind {
  ; TODO: CPR-940 Incorrect codegen add	@const+32[0], r0, r1
  %1 = load i256, ptr addrspace(4) getelementptr inbounds ([10 x i256], ptr addrspace(4) @const, i256 0, i256 1), align 32
	; CHECK: add @const+32[0], r0, stack[@val+1]
  ; TODO: Should be folded into a single instruction.
  store i256 %1, ptr getelementptr inbounds ([10 x i256], ptr @val, i256 0, i256 1), align 32
  ret void
}

; CHECK-LABEL: consti_loadglobal_storeglobal
define void @consti_loadglobal_storeglobal() nounwind {
  %1 = load i256, ptr getelementptr inbounds ([10 x i256], ptr @val, i256 0, i256 7), align 32
  ; CHECK: add stack[@val+7], r0, stack[@val+1]
  store i256 %1, ptr getelementptr inbounds ([10 x i256], ptr @val, i256 0, i256 1), align 32
  ret void
}

; CHECK-LABEL: vari_loadconst_storeglobal
define void @vari_loadconst_storeglobal(i256 %i) nounwind {
  ; TODO: CPR-940 Incorrect codegen add	@const(r1)[0], r0, r2
  %addrc = getelementptr inbounds [10 x i256], ptr addrspace(4) @const, i256 0, i256 %i
  %addrg = getelementptr inbounds [10 x i256], ptr @val, i256 0, i256 %i
  %1 = load i256, ptr addrspace(4) %addrc, align 32
  ; CHECK: div.s 32, r1, r1, r0
	; CHECK: add r2, r0, stack[r1 + @val]
  ; TODO: Should be folded into a single instruction.
  store i256 %1, ptr %addrg, align 32
  ret void
}

; CHECK-LABEL: vari_loadglobal_storeglobal
define void @vari_loadglobal_storeglobal(i256 %i, i256 %j) nounwind {
  ; CHECK: div.s 32, r1, r1, r0
  ; CHECK: add stack[r1 + @val], r0, r1
  %addri = getelementptr inbounds [10 x i256], ptr @val, i256 0, i256 %i
  %addrj = getelementptr inbounds [10 x i256], ptr @val, i256 0, i256 %j
  %1 = load i256, ptr %addri, align 32
  ; CHECK: div.s 32, r2, r2, r0
  ; CHECK: add r1, r0, stack[r2 + @val]
  ; TODO: Should be folded into a single instruction.
  store i256 %1, ptr %addrj, align 32
  ret void
}
