; RUN: opt -Oz -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare i256 @__sha3(i8 addrspace(1)*, i256, i1) #2
declare void @__revert(i256, i256, i256) #1
declare void @__return(i256, i256, i256) #1
declare i256 @__exp(i256, i256) #0

define i256 @test_exp_noinline(i256 %a, i256 %b) {
; CHECK: @test_exp_noinline(i256 %a, i256 %b) local_unnamed_addr #[[EXP:[0-9]+]]
; CHECK-NEXT: call fastcc i256 @__exp(i256 %a, i256 %b) #[[NOINLINE:[0-9]+]]

  %res = call i256 @__exp(i256 %a, i256 %b)
  ret i256 %res
}

define void @test_revert_noinline(i256 %a, i256 %b, i256 %c) {
; CHECK: @test_revert_noinline(i256 %a, i256 %b, i256 %c) local_unnamed_addr #[[EXIT:[0-9]+]]
; CHECK-NEXT: call fastcc void @__revert(i256 %a, i256 %b, i256 %c) #[[NOINLINE]]

  call void @__revert(i256 %a, i256 %b, i256 %c)
  ret void
}

define void @test_return_noinline(i256 %a, i256 %b, i256 %c) {
; CHECK: @test_return_noinline(i256 %a, i256 %b, i256 %c) local_unnamed_addr #[[EXIT]]
; CHECK-NEXT: call fastcc void @__return(i256 %a, i256 %b, i256 %c) #[[NOINLINE]]

  call void @__return(i256 %a, i256 %b, i256 %c)
  ret void
}

define i256 @test_sha3_noinline(i8 addrspace(1)* %a, i256 %b, i1 %c) {
; CHECK: @test_sha3_noinline{{.*}} #[[SHA3:[0-9]+]]
; CHECK-NEXT: call fastcc i256 @__sha3(i8 addrspace(1)* %a, i256 %b, i1 %c) #[[NOINLINE]]

  %res = call i256 @__sha3(i8 addrspace(1)* %a, i256 %b, i1 %c)
  ret i256 %res
}

; CHECK: attributes #[[EXP]] = { minsize mustprogress nofree norecurse nosync optsize readnone willreturn }
; CHECK: attributes #[[EXIT]] = { minsize noreturn optsize }
; CHECK: attributes #[[SHA3]] = { argmemonly minsize nofree optsize readonly }
; CHECK: attributes #[[NOINLINE]] = { noinline }

attributes #0 = { nounwind readnone willreturn }
attributes #1 = { noreturn nounwind }
attributes #2 = { argmemonly readonly nofree null_pointer_is_valid }
