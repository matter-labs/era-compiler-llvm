; RUN: llc < %s | FileCheck %s

source_filename = "main"
target datalayout = "e-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; Function Attrs: nounwind
; CHECK-LABEL: Test_18_deployed
define void @Test_18_deployed() local_unnamed_addr #0 {
entry:
  store i256 128, i256 addrspace(1)* inttoptr (i256 64 to i256 addrspace(1)*), align 64
  %0 = load i256, i256 addrspace(2)* inttoptr (i256 224 to i256 addrspace(2)*), align 32
  %.mask = and i256 %0, -26959946667150639794667015087019630673637144422540572481103610249216
  %1 = icmp eq i256 %.mask, -38342552913550655345396379559601660626067194320209696205844970156170984554496
  br i1 %1, label %if.join5, label %revert

revert:                                           ; preds = %entry, %join27, %if.join5, %join31
  ret void

if.join5:                                         ; preds = %entry
  %2 = load i256, i256 addrspace(2)* inttoptr (i256 256 to i256 addrspace(2)*), align 256
  %3 = tail call i256 @llvm.syncvm.sload(i256 0, i256 0)
  %4 = add i256 %3, %2
  tail call void @llvm.syncvm.sstore(i256 %4, i256 0, i256 0)
  %5 = load i256, i256 addrspace(1)* inttoptr (i256 64 to i256 addrspace(1)*), align 64
  %6 = add i256 %5, 32
  store i256 %6, i256 addrspace(1)* inttoptr (i256 64 to i256 addrspace(1)*), align 64
  %7 = tail call i256 @llvm.syncvm.ltflag()
  %8 = and i256 %7, 1
  %.not = icmp eq i256 %8, 0
  br i1 %.not, label %join27, label %revert

join27:                                           ; preds = %if.join5
  %9 = lshr i256 %5, 5
  %10 = getelementptr i256, i256 addrspace(1)* null, i256 %9
  store i256 undef, i256 addrspace(1)* %10, align 32
  %11 = tail call i256 @llvm.syncvm.ltflag()
  %12 = and i256 %11, 1
  %.not33 = icmp eq i256 %12, 0
  br i1 %.not33, label %join31, label %revert

join31:                                           ; preds = %join27
  tail call void @llvm.memcpy.p2i256.p1i256.i256(i256 addrspace(2)* nonnull align 256 dereferenceable(32) inttoptr (i256 256 to i256 addrspace(2)*), i256 addrspace(1)* nonnull align 32 dereferenceable(32) %10, i256 32, i1 false)
  br label %revert
}

; Function Attrs: nounwind
declare i256 @llvm.syncvm.sload(i256, i256) #0

; Function Attrs: nounwind
declare void @llvm.syncvm.sstore(i256, i256, i256) #0

; Function Attrs: nounwind
declare i256 @llvm.syncvm.ltflag() #0

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.memcpy.p2i256.p1i256.i256(i256 addrspace(2)* noalias nocapture writeonly, i256 addrspace(1)* noalias nocapture readonly, i256, i1 immarg) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind willreturn }
