; RUN: opt -passes=eravm-cse -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i1 @test_dce1(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1) {
; CHECK-LABEL: @test_dce1(
; CHECK-NOT:     call i256 @__system_request
entry:
  %val = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  ret i1 true
}

define i1 @test_dce2(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1) {
; CHECK-LABEL: @test_dce2(
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  %cmp = icmp eq i256 %val1, %val2
  ret i1 %cmp
}

define i256 @test_sig_balance_noelim1(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_balance_noelim1(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @dummy()
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_balance_noelim2(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_balance_noelim2(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call i256(i256*, i256, ...) @llvm.eravm.nearcall(ptr @dummy, i256 42)
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_balance_noelim3(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_balance_noelim3(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__farcall(i256 0, i256 0, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_balance_noelim4(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i1 %cond) {
; CHECK-LABEL: @test_sig_balance_noelim4(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @__system_request
; CHECK:       ret:
; CHECK-NEXT:    call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  br i1 %cond, label %then, label %else

then:
  call void @dummy()
  br label %ret

else:
  br label %ret

ret:
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_sig_balance_noelim5(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i256 %loop_size, i1 %cond) {
; CHECK-LABEL: @test_sig_balance_noelim5(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @__system_request
; CHECK:       loop:
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  br i1 %cond, label %loop, label %ret

loop:
  %phi1 = phi i256 [ %sum, %loop ], [ 0, %entry ]
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @dummy()
  %sum = add nuw nsw i256 %phi1, %val2
  %cmp = icmp slt i256 %sum, %loop_size
  br i1 %cmp, label %loop, label %ret

ret:
  %phi2 = phi i256 [ %val1, %entry ], [ %sum, %loop ]
  ret i256 %phi2
}

define i256 @test_sig_codesize_noelim1(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codesize_noelim1(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @dummy()
  %val2 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codesize_noelim2(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codesize_noelim2(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call i256(i256*, i256, ...) @llvm.eravm.nearcall(ptr @dummy, i256 42)
  %val2 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codesize_noelim3(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codesize_noelim3(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__farcall(i256 0, i256 0, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codehash_noelim1(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codehash_noelim1(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @dummy()
  %val2 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codehash_noelim2(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codehash_noelim2(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call i256(i256*, i256, ...) @llvm.eravm.nearcall(ptr @dummy, i256 42)
  %val2 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codehash_noelim3(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codehash_noelim3(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__farcall(i256 0, i256 0, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_unknown_noelim1(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_unknown_noelim1(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @dummy()
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_unknown_noelim2(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_unknown_noelim2(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call i256(i256*, i256, ...) @llvm.eravm.nearcall(ptr @dummy, i256 42)
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_unknown_noelim3(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_unknown_noelim3(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__farcall(i256 0, i256 0, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_noelim1(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1, ptr %ptr2) {
; CHECK-LABEL: @test_noelim1(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  store i256 1, ptr %ptr2, align 1
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_balance_elim1(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_balance_elim1(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_balance_elim2(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_balance_elim2(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @llvm.eravm.setu128(i256 1)
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_balance_elim3(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_balance_elim3(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__staticcall(i256 0, i256 1, i256 2, i256 3, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_balance_elim4(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i8 addrspace(3)* %ptr2) {
; CHECK-LABEL: @test_sig_balance_elim4(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__staticcall_byref(i8 addrspace(3)* %ptr2, i256 1, i256 2, i256 3, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_balance_elim5(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i1 %cond) {
; CHECK-LABEL: @test_sig_balance_elim5(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @__system_request
; CHECK:       ret:
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  br i1 %cond, label %then, label %else

then:
  br label %ret

else:
  br label %ret

ret:
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_sig_balance_elim6(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i1 %cond) {
; CHECK-LABEL: @test_sig_balance_elim6(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @__system_request
; CHECK:       ret1:
; CHECK-NEXT:    call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
; CHECK:       ret2:
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  br i1 %cond, label %check, label %ret2

check:
  call void @dummy()
  br label %ret1

ret1:
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %val3 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %sum1 = add i256 %val2, %val3
  %res1 = add i256 %val1, %sum1
  ret i256 %res1

ret2:
  %val4 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %res2 = add i256 %val1, %val4
  ret i256 %res2
}

define i256 @test_sig_balance_elim7(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i1 %cond) {
; CHECK-LABEL: @test_sig_balance_elim7(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @__system_request
; CHECK:       ret1:
; CHECK-NOT:     call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
; CHECK:       ret2:
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  br i1 %cond, label %check, label %ret2

check:
  call void @llvm.eravm.setu128(i256 1)
  br label %ret1

ret1:
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %val3 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %sum1 = add i256 %val2, %val3
  %res1 = add i256 %val1, %sum1
  ret i256 %res1

ret2:
  %val4 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %res2 = add i256 %val1, %val4
  ret i256 %res2
}

define i256 @test_sig_balance_elim8(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i1 %cond1, i1 %cond2) {
; CHECK-LABEL: @test_sig_balance_elim8(
; CHECK:       entry:
; CHECK-NEXT:    call i256 @__system_request
; CHECK:       ret1:
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  br i1 %cond1, label %then, label %else

then:
  br label %ret1

else:
  br i1 %cond2, label %ret1, label %ret2

ret2:
  call void @dummy()
  ret i256 0

ret1:
  %val2 = call i256 @__system_request(i256 %address, i256 -44877977326897262784168444354156441158329539312518651612887364914072161059015, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %res = add i256 %val1, %val2
  ret i256 %res
}

define i256 @test_sig_codesize_elim1(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codesize_elim1(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %val2 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codesize_elim2(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codesize_elim2(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @llvm.eravm.setu128(i256 1)
  %val2 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codesize_elim3(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codesize_elim3(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__staticcall(i256 0, i256 1, i256 2, i256 3, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codesize_elim4(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i8 addrspace(3)* %ptr2) {
; CHECK-LABEL: @test_sig_codesize_elim4(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__staticcall_byref(i8 addrspace(3)* %ptr2, i256 1, i256 2, i256 3, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 10867283408178898638301172343726954674910073630256871736220740970449699113859, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codehash_elim1(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codehash_elim1(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %val2 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codehash_elim2(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codehash_elim2(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @llvm.eravm.setu128(i256 1)
  %val2 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codehash_elim3(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_codehash_elim3(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__staticcall(i256 0, i256 1, i256 2, i256 3, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_codehash_elim4(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i8 addrspace(3)* %ptr2) {
; CHECK-LABEL: @test_sig_codehash_elim4(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__staticcall_byref(i8 addrspace(3)* %ptr2, i256 1, i256 2, i256 3, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 -14361143668726333047832245035021462516528029169938439142256918440275001541106, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_unknown_elim1(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_unknown_elim1(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_unknown_elim2(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_unknown_elim2(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @llvm.eravm.setu128(i256 1)
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_unknown_elim3(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_unknown_elim3(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__staticcall(i256 0, i256 1, i256 2, i256 3, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_unknown_elim4(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1, i8 addrspace(3)* %ptr2) {
; CHECK-LABEL: @test_sig_unknown_elim4(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__staticcall_byref(i8 addrspace(3)* %ptr2, i256 1, i256 2, i256 3, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_other_elim1(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_other_elim1(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -57577886008109372094702712535079357197429613162988981639842156834261218611805, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call void @dummy()
  %val2 = call i256 @__system_request(i256 %address, i256 -57577886008109372094702712535079357197429613162988981639842156834261218611805, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_other_elim2(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_other_elim2(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -57577886008109372094702712535079357197429613162988981639842156834261218611805, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call i256(i256*, i256, ...) @llvm.eravm.nearcall(ptr @dummy, i256 42)
  %val2 = call i256 @__system_request(i256 %address, i256 -57577886008109372094702712535079357197429613162988981639842156834261218611805, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_sig_other_elim3(i256 %address, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1) {
; CHECK-LABEL: @test_sig_other_elim3(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 -57577886008109372094702712535079357197429613162988981639842156834261218611805, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  call {i8 addrspace(3)*, i1} @__farcall(i256 0, i256 0, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef, i256 undef)
  %val2 = call i256 @__system_request(i256 %address, i256 -57577886008109372094702712535079357197429613162988981639842156834261218611805, i256 %size, ptr noalias nocapture nofree noundef nonnull align 32 %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_elim1(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1) {
; CHECK-LABEL: @test_elim1(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  store i256 1, ptr addrspace(1) null, align 4294967296
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

define i256 @test_elim2(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1, ptr %ptr2) {
; CHECK-LABEL: @test_elim2(
; CHECK:         call i256 @__system_request
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  store i256 1, ptr %ptr2, align 1
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  %val3 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  %sum = add i256 %val1, %val2
  %ret = add i256 %sum, %val3
  ret i256 %ret
}

define i256 @test_elim3(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1, i256 %key, i256 %val) {
; CHECK-LABEL: @test_elim3(
; CHECK:         call i256 @__system_request
; CHECK-NOT:     call i256 @__system_request
;
entry:
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  call void @llvm.eravm.event(i256 %key, i256 %val, i256 0)
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 %size, ptr nocapture readonly %ptr1)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

; TODO: CPR-1509 Eliminate calls based on the input data.
define i256 @test_elim4(i256 %address, i256 %sig, i256 %input_data) {
; CHECK-LABEL: @test_elim4(
; CHECK:         call i256 @__system_request
; CHECK:       bb1:
; CHECK:         call i256 @__system_request
;
entry:
  %alloca1 = alloca [1 x i256], align 32
  store i256 %input_data, ptr %alloca1, align 32
  %val1 = call i256 @__system_request(i256 %address, i256 %sig, i256 36, ptr noalias nocapture nofree noundef nonnull align 32 %alloca1)
  br label %bb1

bb1:
  %alloca2 = alloca [1 x i256], align 32
  store i256 %input_data, ptr %alloca2, align 32
  %val2 = call i256 @__system_request(i256 %address, i256 %sig, i256 36, ptr noalias nocapture nofree noundef nonnull align 32 %alloca2)
  %ret = add i256 %val1, %val2
  ret i256 %ret
}

declare void @dummy()
declare i256 @__system_request(i256, i256, i256, ptr) #0
declare {i8 addrspace(3)*, i1} @__farcall(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256)
declare {i8 addrspace(3)*, i1} @__staticcall(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256)
declare {i8 addrspace(3)*, i1} @__staticcall_byref(i8 addrspace(3)*, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256)
declare void @llvm.eravm.setu128(i256)
declare void @llvm.eravm.event(i256, i256, i256)
declare i256 @llvm.eravm.nearcall(i256*, i256, ...)

attributes #0 = { argmemonly nofree null_pointer_is_valid readonly }
