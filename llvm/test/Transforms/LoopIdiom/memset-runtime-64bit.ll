; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt -passes="function(loop(indvars,loop-idiom,loop-deletion),simplifycfg)" -S < %s | FileCheck %s
; XFAIL: target=eravm{{.*}}, target=evm{{.*}}
; Compile command:
; $ clang -m64 -fno-discard-value-names -O0 -S -emit-llvm -Xclang -disable-O0-optnone Code.c
; $ bin/opt -S -passes=mem2reg,loop-simplify,lcssa,loop-rotate \
; -passes=licm,simple-loop-unswitch -enable-nontrivial-unswitch -passes=loop-simplify \
; -passes=loop-deletion,simplifycfg,indvars Code.ll > CodeOpt.ll
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
; void PositiveFor64(int *ar, long long n, long long m)
; {
;   long long i;
;   for (i=0; i<n; ++i) {
;     int *arr = ar + i * m;
;     memset(arr, 0, m * sizeof(int));
;   }
; }
define dso_local void @PositiveFor64(ptr %ar, i64 %n, i64 %m) {
; CHECK-LABEL: @PositiveFor64(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP1:%.*]] = icmp slt i64 0, [[N:%.*]]
; CHECK-NEXT:    br i1 [[CMP1]], label [[FOR_BODY_LR_PH:%.*]], label [[FOR_END:%.*]]
; CHECK:       for.body.lr.ph:
; CHECK-NEXT:    [[MUL1:%.*]] = mul i64 [[M:%.*]], 4
; CHECK-NEXT:    [[TMP0:%.*]] = mul i64 [[M]], [[N]]
; CHECK-NEXT:    [[TMP1:%.*]] = shl i64 [[TMP0]], 2
; CHECK-NEXT:    call void @llvm.memset.p0.i64(ptr align 4 [[AR:%.*]], i8 0, i64 [[TMP1]], i1 false)
; CHECK-NEXT:    br label [[FOR_END]]
; CHECK:       for.end:
; CHECK-NEXT:    ret void
;
entry:
  %cmp1 = icmp slt i64 0, %n
  br i1 %cmp1, label %for.body.lr.ph, label %for.end

for.body.lr.ph:                                   ; preds = %entry
  %mul1 = mul i64 %m, 4
  br label %for.body

for.body:                                         ; preds = %for.body.lr.ph, %for.body
  %i.02 = phi i64 [ 0, %for.body.lr.ph ], [ %inc, %for.body ]
  %mul = mul nsw i64 %i.02, %m
  %add.ptr = getelementptr inbounds i32, ptr %ar, i64 %mul
  call void @llvm.memset.p0.i64(ptr align 4 %add.ptr, i8 0, i64 %mul1, i1 false)
  %inc = add nsw i64 %i.02, 1
  %cmp = icmp slt i64 %inc, %n
  br i1 %cmp, label %for.body, label %for.end

for.end:                                          ; preds = %for.body, %entry
  ret void
}
; void NegativeFor64(int *ar, long long n, long long m)
; {
;   long long i;
;   for (i=n-1; i>=0; --i) {
;     int *arr = ar + i * m;
;     memset(arr, 0, m * sizeof(int));
;   }
; }
define dso_local void @NegativeFor64(ptr %ar, i64 %n, i64 %m) {
; CHECK-LABEL: @NegativeFor64(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[SUB:%.*]] = sub nsw i64 [[N:%.*]], 1
; CHECK-NEXT:    [[CMP1:%.*]] = icmp sge i64 [[SUB]], 0
; CHECK-NEXT:    br i1 [[CMP1]], label [[FOR_BODY_LR_PH:%.*]], label [[FOR_END:%.*]]
; CHECK:       for.body.lr.ph:
; CHECK-NEXT:    [[MUL1:%.*]] = mul i64 [[M:%.*]], 4
; CHECK-NEXT:    [[TMP0:%.*]] = mul i64 [[M]], [[N]]
; CHECK-NEXT:    [[TMP1:%.*]] = shl i64 [[TMP0]], 2
; CHECK-NEXT:    call void @llvm.memset.p0.i64(ptr align 4 [[AR:%.*]], i8 0, i64 [[TMP1]], i1 false)
; CHECK-NEXT:    br label [[FOR_END]]
; CHECK:       for.end:
; CHECK-NEXT:    ret void
;
entry:
  %sub = sub nsw i64 %n, 1
  %cmp1 = icmp sge i64 %sub, 0
  br i1 %cmp1, label %for.body.lr.ph, label %for.end

for.body.lr.ph:                                   ; preds = %entry
  %mul1 = mul i64 %m, 4
  br label %for.body

for.body:                                         ; preds = %for.body.lr.ph, %for.body
  %i.02 = phi i64 [ %sub, %for.body.lr.ph ], [ %dec, %for.body ]
  %mul = mul nsw i64 %i.02, %m
  %add.ptr = getelementptr inbounds i32, ptr %ar, i64 %mul
  call void @llvm.memset.p0.i64(ptr align 4 %add.ptr, i8 0, i64 %mul1, i1 false)
  %dec = add nsw i64 %i.02, -1
  %cmp = icmp sge i64 %dec, 0
  br i1 %cmp, label %for.body, label %for.end

for.end:                                          ; preds = %for.body, %entry
  ret void
}
; void NestedFor64(int *ar, long long n, long long m, long long o)
; {
;   long long i, j;
;   for (i=0; i<n; ++i) {
;     for (j=0; j<m; j++) {
;       int *arr = ar + i * m * o + j * o;
;       memset(arr, 0, o * sizeof(int));
;     }
;   }
; }
define void @NestedFor64(ptr %ar, i64 %n, i64 %m, i64 %o) {
; CHECK-LABEL: @NestedFor64(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP3:%.*]] = icmp slt i64 0, [[N:%.*]]
; CHECK-NEXT:    [[CMP21:%.*]] = icmp slt i64 0, [[M:%.*]]
; CHECK-NEXT:    [[MUL7:%.*]] = mul i64 [[O:%.*]], 4
; CHECK-NEXT:    [[OR_COND:%.*]] = select i1 [[CMP3]], i1 [[CMP21]], i1 false
; CHECK-NEXT:    br i1 [[OR_COND]], label [[FOR_BODY_US_PREHEADER:%.*]], label [[FOR_END10:%.*]]
; CHECK:       for.body.us.preheader:
; CHECK-NEXT:    [[TMP0:%.*]] = mul i64 [[O]], [[M]]
; CHECK-NEXT:    [[TMP1:%.*]] = shl i64 [[TMP0]], 2
; CHECK-NEXT:    [[TMP2:%.*]] = mul i64 [[TMP0]], [[N]]
; CHECK-NEXT:    [[TMP3:%.*]] = shl i64 [[TMP2]], 2
; CHECK-NEXT:    call void @llvm.memset.p0.i64(ptr align 4 [[AR:%.*]], i8 0, i64 [[TMP3]], i1 false)
; CHECK-NEXT:    br label [[FOR_END10]]
; CHECK:       for.end10:
; CHECK-NEXT:    ret void
;
entry:
  %cmp3 = icmp slt i64 0, %n
  br i1 %cmp3, label %for.body.lr.ph, label %for.end10

for.body.lr.ph:                                   ; preds = %entry
  %cmp21 = icmp slt i64 0, %m
  %mul7 = mul i64 %o, 4
  br i1 %cmp21, label %for.body.us.preheader, label %for.end10

for.body.us.preheader:                            ; preds = %for.body.lr.ph
  br label %for.body.us

for.body.us:                                      ; preds = %for.body.us.preheader, %for.cond1.for.end_crit_edge.us
  %i.04.us = phi i64 [ %inc9.us, %for.cond1.for.end_crit_edge.us ], [ 0, %for.body.us.preheader ]
  %mul.us = mul nsw i64 %i.04.us, %m
  %mul4.us = mul nsw i64 %mul.us, %o
  %add.ptr.us = getelementptr inbounds i32, ptr %ar, i64 %mul4.us
  br label %for.body3.us

for.body3.us:                                     ; preds = %for.body.us, %for.body3.us
  %j.02.us = phi i64 [ 0, %for.body.us ], [ %inc.us, %for.body3.us ]
  %mul5.us = mul nsw i64 %j.02.us, %o
  %add.ptr6.us = getelementptr inbounds i32, ptr %add.ptr.us, i64 %mul5.us
  call void @llvm.memset.p0.i64(ptr align 4 %add.ptr6.us, i8 0, i64 %mul7, i1 false)
  %inc.us = add nuw nsw i64 %j.02.us, 1
  %exitcond = icmp ne i64 %inc.us, %m
  br i1 %exitcond, label %for.body3.us, label %for.cond1.for.end_crit_edge.us

for.cond1.for.end_crit_edge.us:                   ; preds = %for.body3.us
  %inc9.us = add nuw nsw i64 %i.04.us, 1
  %exitcond5 = icmp ne i64 %inc9.us, %n
  br i1 %exitcond5, label %for.body.us, label %for.end10.loopexit

for.end10.loopexit:                               ; preds = %for.cond1.for.end_crit_edge.us
  br label %for.end10

for.end10:                                        ; preds = %for.end10.loopexit, %for.body.lr.ph, %entry
  ret void
}
; void PositiveFor32(int *ar, int n, int m)
; {
;   int i;
;   for (i=0; i<n; ++i) {
;     int *arr = ar + i * m;
;     memset(arr, 0, m * sizeof(int));
;   }
; }
define void @PositiveFor32(ptr %ar, i32 %n, i32 %m) {
; CHECK-LABEL: @PositiveFor32(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CONV:%.*]] = sext i32 [[N:%.*]] to i64
; CHECK-NEXT:    [[CMP1:%.*]] = icmp slt i64 0, [[CONV]]
; CHECK-NEXT:    br i1 [[CMP1]], label [[FOR_BODY_LR_PH:%.*]], label [[FOR_END:%.*]]
; CHECK:       for.body.lr.ph:
; CHECK-NEXT:    [[CONV1:%.*]] = sext i32 [[M:%.*]] to i64
; CHECK-NEXT:    [[CONV2:%.*]] = sext i32 [[M]] to i64
; CHECK-NEXT:    [[MUL3:%.*]] = mul i64 [[CONV2]], 4
; CHECK-NEXT:    [[TMP0:%.*]] = mul i64 [[CONV1]], [[CONV]]
; CHECK-NEXT:    [[TMP1:%.*]] = shl i64 [[TMP0]], 2
; CHECK-NEXT:    call void @llvm.memset.p0.i64(ptr align 4 [[AR:%.*]], i8 0, i64 [[TMP1]], i1 false)
; CHECK-NEXT:    br label [[FOR_END]]
; CHECK:       for.end:
; CHECK-NEXT:    ret void
;
entry:
  %conv = sext i32 %n to i64
  %cmp1 = icmp slt i64 0, %conv
  br i1 %cmp1, label %for.body.lr.ph, label %for.end

for.body.lr.ph:                                   ; preds = %entry
  %conv1 = sext i32 %m to i64
  %conv2 = sext i32 %m to i64
  %mul3 = mul i64 %conv2, 4
  br label %for.body

for.body:                                         ; preds = %for.body.lr.ph, %for.body
  %i.02 = phi i64 [ 0, %for.body.lr.ph ], [ %inc, %for.body ]
  %mul = mul nsw i64 %i.02, %conv1
  %add.ptr = getelementptr inbounds i32, ptr %ar, i64 %mul
  call void @llvm.memset.p0.i64(ptr align 4 %add.ptr, i8 0, i64 %mul3, i1 false)
  %inc = add nsw i64 %i.02, 1
  %cmp = icmp slt i64 %inc, %conv
  br i1 %cmp, label %for.body, label %for.end

for.end:                                          ; preds = %for.body, %entry
  ret void
}
; void Negative32(int *ar, int n, int m)
; {
;   long long i;
;   for (i=n-1; i>=0; i--) {
;     int *arr = ar + i * m;
;     memset(arr, 0, m * sizeof(int));
;   }
; }
define void @Negative32(ptr %ar, i32 %n, i32 %m) {
; CHECK-LABEL: @Negative32(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[SUB:%.*]] = sub nsw i32 [[N:%.*]], 1
; CHECK-NEXT:    [[CONV:%.*]] = sext i32 [[SUB]] to i64
; CHECK-NEXT:    [[CMP1:%.*]] = icmp sge i64 [[CONV]], 0
; CHECK-NEXT:    br i1 [[CMP1]], label [[FOR_BODY_LR_PH:%.*]], label [[FOR_END:%.*]]
; CHECK:       for.body.lr.ph:
; CHECK-NEXT:    [[CONV1:%.*]] = sext i32 [[M:%.*]] to i64
; CHECK-NEXT:    [[CONV2:%.*]] = sext i32 [[M]] to i64
; CHECK-NEXT:    [[MUL3:%.*]] = mul i64 [[CONV2]], 4
; CHECK-NEXT:    [[TMP0:%.*]] = sext i32 [[N]] to i64
; CHECK-NEXT:    [[TMP1:%.*]] = mul i64 [[CONV1]], [[TMP0]]
; CHECK-NEXT:    [[TMP2:%.*]] = shl i64 [[TMP1]], 2
; CHECK-NEXT:    call void @llvm.memset.p0.i64(ptr align 4 [[AR:%.*]], i8 0, i64 [[TMP2]], i1 false)
; CHECK-NEXT:    br label [[FOR_END]]
; CHECK:       for.end:
; CHECK-NEXT:    ret void
;
entry:
  %sub = sub nsw i32 %n, 1
  %conv = sext i32 %sub to i64
  %cmp1 = icmp sge i64 %conv, 0
  br i1 %cmp1, label %for.body.lr.ph, label %for.end

for.body.lr.ph:                                   ; preds = %entry
  %conv1 = sext i32 %m to i64
  %conv2 = sext i32 %m to i64
  %mul3 = mul i64 %conv2, 4
  br label %for.body

for.body:                                         ; preds = %for.body.lr.ph, %for.body
  %i.02 = phi i64 [ %conv, %for.body.lr.ph ], [ %dec, %for.body ]
  %mul = mul nsw i64 %i.02, %conv1
  %add.ptr = getelementptr inbounds i32, ptr %ar, i64 %mul
  call void @llvm.memset.p0.i64(ptr align 4 %add.ptr, i8 0, i64 %mul3, i1 false)
  %dec = add nsw i64 %i.02, -1
  %cmp = icmp sge i64 %dec, 0
  br i1 %cmp, label %for.body, label %for.end

for.end:                                          ; preds = %for.body, %entry
  ret void
}
; This case requires SCEVFolder in LoopIdiomRecognize.cpp to fold SCEV prior to comparison.
; For the inner-loop, SCEVFolder is not needed, however the promoted memset size would be based
; on the trip count of inner-loop (which is an unsigned integer).
; Then in the outer loop, the pointer stride SCEV for memset needs to be converted based on the
; loop guard for it to equal to the memset size SCEV. The loop guard guaranteeds that m >= 0
; inside the loop, so m can be converted from sext to zext, making the two SCEV-s equal.
; void NestedFor32(int *ar, int n, int m, int o)
; {
;   int i, j;
;   for (i=0; i<n; ++i) {
;     for (j=0; j<m; j++) {
;       int *arr = ar + i * m * o + j * o;
;       memset(arr, 0, o * sizeof(int));
;     }
;   }
; }
define void @NestedFor32(ptr %ar, i32 %n, i32 %m, i32 %o) {
; CHECK-LABEL: @NestedFor32(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP3:%.*]] = icmp slt i32 0, [[N:%.*]]
; CHECK-NEXT:    br i1 [[CMP3]], label [[FOR_BODY_LR_PH:%.*]], label [[FOR_END11:%.*]]
; CHECK:       for.body.lr.ph:
; CHECK-NEXT:    [[CMP21:%.*]] = icmp slt i32 0, [[M:%.*]]
; CHECK-NEXT:    [[CONV:%.*]] = sext i32 [[O:%.*]] to i64
; CHECK-NEXT:    [[MUL8:%.*]] = mul i64 [[CONV]], 4
; CHECK-NEXT:    br i1 [[CMP21]], label [[FOR_BODY_US_PREHEADER:%.*]], label [[FOR_END11]]
; CHECK:       for.body.us.preheader:
; CHECK-NEXT:    [[TMP0:%.*]] = sext i32 [[O]] to i64
; CHECK-NEXT:    [[TMP1:%.*]] = sext i32 [[M]] to i64
; CHECK-NEXT:    [[WIDE_TRIP_COUNT10:%.*]] = zext i32 [[N]] to i64
; CHECK-NEXT:    [[TMP2:%.*]] = mul i64 [[TMP0]], [[TMP1]]
; CHECK-NEXT:    [[TMP3:%.*]] = shl i64 [[TMP2]], 2
; CHECK-NEXT:    [[TMP4:%.*]] = zext i32 [[M]] to i64
; CHECK-NEXT:    [[TMP5:%.*]] = mul i64 [[TMP0]], [[TMP4]]
; CHECK-NEXT:    [[TMP6:%.*]] = shl i64 [[TMP5]], 2
; CHECK-NEXT:    [[TMP7:%.*]] = mul i64 [[TMP5]], [[WIDE_TRIP_COUNT10]]
; CHECK-NEXT:    [[TMP8:%.*]] = shl i64 [[TMP7]], 2
; CHECK-NEXT:    call void @llvm.memset.p0.i64(ptr align 4 [[AR:%.*]], i8 0, i64 [[TMP8]], i1 false)
; CHECK-NEXT:    br label [[FOR_END11]]
; CHECK:       for.end11:
; CHECK-NEXT:    ret void
;
entry:
  %cmp3 = icmp slt i32 0, %n
  br i1 %cmp3, label %for.body.lr.ph, label %for.end11

for.body.lr.ph:                                   ; preds = %entry
  %cmp21 = icmp slt i32 0, %m
  %conv = sext i32 %o to i64
  %mul8 = mul i64 %conv, 4
  br i1 %cmp21, label %for.body.us.preheader, label %for.end11

for.body.us.preheader:                            ; preds = %for.body.lr.ph
  %0 = sext i32 %o to i64
  %1 = sext i32 %m to i64
  %2 = sext i32 %o to i64
  %wide.trip.count10 = zext i32 %n to i64
  br label %for.body.us

for.body.us:                                      ; preds = %for.body.us.preheader, %for.cond1.for.end_crit_edge.us
  %indvars.iv6 = phi i64 [ 0, %for.body.us.preheader ], [ %indvars.iv.next7, %for.cond1.for.end_crit_edge.us ]
  %3 = mul nsw i64 %indvars.iv6, %1
  %4 = mul nsw i64 %3, %2
  %add.ptr.us = getelementptr inbounds i32, ptr %ar, i64 %4
  %wide.trip.count = zext i32 %m to i64
  br label %for.body3.us

for.body3.us:                                     ; preds = %for.body.us, %for.body3.us
  %indvars.iv = phi i64 [ 0, %for.body.us ], [ %indvars.iv.next, %for.body3.us ]
  %5 = mul nsw i64 %indvars.iv, %0
  %add.ptr7.us = getelementptr inbounds i32, ptr %add.ptr.us, i64 %5
  call void @llvm.memset.p0.i64(ptr align 4 %add.ptr7.us, i8 0, i64 %mul8, i1 false)
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp ne i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond, label %for.body3.us, label %for.cond1.for.end_crit_edge.us

for.cond1.for.end_crit_edge.us:                   ; preds = %for.body3.us
  %indvars.iv.next7 = add nuw nsw i64 %indvars.iv6, 1
  %exitcond11 = icmp ne i64 %indvars.iv.next7, %wide.trip.count10
  br i1 %exitcond11, label %for.body.us, label %for.end11.loopexit

for.end11.loopexit:                               ; preds = %for.cond1.for.end_crit_edge.us
  br label %for.end11

for.end11:                                        ; preds = %for.end11.loopexit, %for.body.lr.ph, %entry
  ret void
}

; void NegStart(int n, int m, int *ar) {
;   for (int i = -100; i < n; i++) {
;     int *arr = ar + (i + 100) * m;
;     memset(arr, 0, m * sizeof(int));
;   }
; }
define void @NegStart(i32 %n, i32 %m, ptr %ar) {
; CHECK-LABEL: @NegStart(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP1:%.*]] = icmp slt i32 -100, [[N:%.*]]
; CHECK-NEXT:    br i1 [[CMP1]], label [[FOR_BODY_LR_PH:%.*]], label [[FOR_END:%.*]]
; CHECK:       for.body.lr.ph:
; CHECK-NEXT:    [[CONV:%.*]] = sext i32 [[M:%.*]] to i64
; CHECK-NEXT:    [[MUL1:%.*]] = mul i64 [[CONV]], 4
; CHECK-NEXT:    [[TMP0:%.*]] = sext i32 [[M]] to i64
; CHECK-NEXT:    [[WIDE_TRIP_COUNT:%.*]] = sext i32 [[N]] to i64
; CHECK-NEXT:    [[TMP1:%.*]] = add nsw i64 [[WIDE_TRIP_COUNT]], 100
; CHECK-NEXT:    [[TMP2:%.*]] = mul i64 [[TMP1]], [[TMP0]]
; CHECK-NEXT:    [[TMP3:%.*]] = shl i64 [[TMP2]], 2
; CHECK-NEXT:    call void @llvm.memset.p0.i64(ptr align 4 [[AR:%.*]], i8 0, i64 [[TMP3]], i1 false)
; CHECK-NEXT:    br label [[FOR_END]]
; CHECK:       for.end:
; CHECK-NEXT:    ret void
;
entry:
  %cmp1 = icmp slt i32 -100, %n
  br i1 %cmp1, label %for.body.lr.ph, label %for.end

for.body.lr.ph:                                   ; preds = %entry
  %conv = sext i32 %m to i64
  %mul1 = mul i64 %conv, 4
  %0 = sext i32 %m to i64
  %wide.trip.count = sext i32 %n to i64
  br label %for.body

for.body:                                         ; preds = %for.body.lr.ph, %for.body
  %indvars.iv = phi i64 [ -100, %for.body.lr.ph ], [ %indvars.iv.next, %for.body ]
  %1 = add nsw i64 %indvars.iv, 100
  %2 = mul nsw i64 %1, %0
  %add.ptr = getelementptr inbounds i32, ptr %ar, i64 %2
  call void @llvm.memset.p0.i64(ptr align 4 %add.ptr, i8 0, i64 %mul1, i1 false)
  %indvars.iv.next = add nsw i64 %indvars.iv, 1
  %exitcond = icmp ne i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond, label %for.body, label %for.end.loopexit

for.end.loopexit:                                 ; preds = %for.body
  br label %for.end

for.end:                                          ; preds = %for.end.loopexit, %entry
  ret void
}

declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg)
