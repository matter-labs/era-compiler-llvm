; RUN: opt -S -mtriple=eravm -passes="mergebb,simplifycfg<switch-range-to-icmp>" < %s | FileCheck %s

declare void @dummy()

define i32 @basic(i32 %x, i32* %p) {
; CHECK-LABEL: @basic(
; CHECK-NEXT:    [[SWITCH:%.*]] = icmp ult i32 [[X:%.*]], 2
; CHECK-NEXT:    br i1 [[SWITCH]], label [[BB1:%.*]], label [[BB3:%.*]]
; CHECK:       bb1:
; CHECK-NEXT:    store i32 0, i32* [[P:%.*]], align 4
; CHECK-NEXT:    br label [[EXIT:%.*]]
; CHECK:       bb3:
; CHECK-NEXT:    call void @dummy()
; CHECK-NEXT:    br label [[EXIT]]
; CHECK:       exit:
; CHECK-NEXT:    [[PHI:%.*]] = phi i32 [ 0, [[BB1]] ], [ 1, [[BB3]] ]
; CHECK-NEXT:    ret i32 [[PHI]]
;
  switch i32 %x, label %bb3 [
  i32 0, label %bb1
  i32 1, label %bb2
  ]

bb1:
  store i32 0, i32* %p
  br label %exit

bb2:
  store i32 0, i32* %p
  br label %exit

bb3:
  call void @dummy()
  br label %exit

exit:
  %phi = phi i32 [ 0, %bb1 ], [ 0, %bb2 ], [ 1, %bb3]
  ret i32 %phi
}

; nonnull present in block blocks, keep it.
define i32 @metadata_nonnull_keep(i32 %x, i32** %p1, i32** %p2) {
; CHECK-LABEL: @metadata_nonnull_keep(
; CHECK-NEXT:    [[SWITCH:%.*]] = icmp ult i32 [[X:%.*]], 2
; CHECK-NEXT:    br i1 [[SWITCH]], label [[BB1:%.*]], label [[BB3:%.*]]
; CHECK:       bb1:
; CHECK-NEXT:    [[V1:%.*]] = load i32*, i32** [[P1:%.*]], align 4, !nonnull !0
; CHECK-NEXT:    store i32* [[V1]], i32** [[P2:%.*]], align 8
; CHECK-NEXT:    br label [[EXIT:%.*]]
; CHECK:       bb3:
; CHECK-NEXT:    call void @dummy()
; CHECK-NEXT:    br label [[EXIT]]
; CHECK:       exit:
; CHECK-NEXT:    [[PHI:%.*]] = phi i32 [ 0, [[BB1]] ], [ 1, [[BB3]] ]
; CHECK-NEXT:    ret i32 [[PHI]]
;
  switch i32 %x, label %bb3 [
  i32 0, label %bb1
  i32 1, label %bb2
  ]

bb1:
  %v1 = load i32*, i32** %p1, align 4, !nonnull !{}
  store i32* %v1, i32** %p2
  br label %exit

bb2:
  %v2 = load i32*, i32** %p1, align 4, !nonnull !{}
  store i32* %v2, i32** %p2
  br label %exit

bb3:
  call void @dummy()
  br label %exit

exit:
  %phi = phi i32 [ 0, %bb1 ], [ 0, %bb2 ], [ 1, %bb3]
  ret i32 %phi
}

; nonnull only present in one of the blocks, drop it.
define i32 @metadata_nonnull_drop(i32 %x, i32** %p1, i32** %p2) {
; CHECK-LABEL: @metadata_nonnull_drop(
; CHECK-NEXT:    [[SWITCH:%.*]] = icmp ult i32 [[X:%.*]], 2
; CHECK-NEXT:    br i1 [[SWITCH]], label [[BB1:%.*]], label [[BB3:%.*]]
; CHECK:       bb1:
; CHECK-NEXT:    [[V1:%.*]] = load i32*, i32** [[P1:%.*]], align 4
; CHECK-NEXT:    store i32* [[V1]], i32** [[P2:%.*]], align 8
; CHECK-NEXT:    br label [[EXIT:%.*]]
; CHECK:       bb3:
; CHECK-NEXT:    call void @dummy()
; CHECK-NEXT:    br label [[EXIT]]
; CHECK:       exit:
; CHECK-NEXT:    [[PHI:%.*]] = phi i32 [ 0, [[BB1]] ], [ 1, [[BB3]] ]
; CHECK-NEXT:    ret i32 [[PHI]]
;
  switch i32 %x, label %bb3 [
  i32 0, label %bb1
  i32 1, label %bb2
  ]

bb1:
  %v1 = load i32*, i32** %p1, align 4, !nonnull !{}
  store i32* %v1, i32** %p2
  br label %exit

bb2:
  %v2 = load i32*, i32** %p1, align 4
  store i32* %v2, i32** %p2
  br label %exit

bb3:
  call void @dummy()
  br label %exit

exit:
  %phi = phi i32 [ 0, %bb1 ], [ 0, %bb2 ], [ 1, %bb3]
  ret i32 %phi
}

; The union of both range metadatas should be taken.
define i32 @metadata_range(i32 %x, i32* %p1, i32* %p2) {
; CHECK-LABEL: @metadata_range(
; CHECK-NEXT:    [[SWITCH:%.*]] = icmp ult i32 [[X:%.*]], 2
; CHECK-NEXT:    br i1 [[SWITCH]], label [[BB1:%.*]], label [[BB3:%.*]]
; CHECK:       bb1:
; CHECK-NEXT:    [[V1:%.*]] = load i32, i32* [[P1:%.*]], align 4, !range !1
; CHECK-NEXT:    store i32 [[V1]], i32* [[P2:%.*]], align 4
; CHECK-NEXT:    br label [[EXIT:%.*]]
; CHECK:       bb3:
; CHECK-NEXT:    call void @dummy()
; CHECK-NEXT:    br label [[EXIT]]
; CHECK:       exit:
; CHECK-NEXT:    [[PHI:%.*]] = phi i32 [ 0, [[BB1]] ], [ 1, [[BB3]] ]
; CHECK-NEXT:    ret i32 [[PHI]]
;
  switch i32 %x, label %bb3 [
  i32 0, label %bb1
  i32 1, label %bb2
  ]

bb1:
  %v1 = load i32, i32* %p1, align 4, !range !{i32 0, i32 10}
  store i32 %v1, i32* %p2
  br label %exit

bb2:
  %v2 = load i32, i32* %p1, align 4, !range !{i32 5, i32 15}
  store i32 %v2, i32* %p2
  br label %exit

bb3:
  call void @dummy()
  br label %exit

exit:
  %phi = phi i32 [ 0, %bb1 ], [ 0, %bb2 ], [ 1, %bb3]
  ret i32 %phi
}

; Only the common nuw flag may be preserved.
define i32 @attributes(i32 %x, i32 %y) {
; CHECK-LABEL: @attributes(
; CHECK-NEXT:    [[SWITCH:%.*]] = icmp ult i32 [[X:%.*]], 2
; CHECK-NEXT:    br i1 [[SWITCH]], label [[BB1:%.*]], label [[BB3:%.*]]
; CHECK:       bb1:
; CHECK-NEXT:    [[A:%.*]] = add nuw i32 [[Y:%.*]], 1
; CHECK-NEXT:    br label [[EXIT:%.*]]
; CHECK:       bb3:
; CHECK-NEXT:    call void @dummy()
; CHECK-NEXT:    br label [[EXIT]]
; CHECK:       exit:
; CHECK-NEXT:    [[PHI:%.*]] = phi i32 [ [[A]], [[BB1]] ], [ 1, [[BB3]] ]
; CHECK-NEXT:    ret i32 [[PHI]]
;
  switch i32 %x, label %bb3 [
  i32 0, label %bb1
  i32 1, label %bb2
  ]

bb1:
  %a = add nuw nsw i32 %y, 1
  br label %exit

bb2:
  %b = add nuw i32 %y, 1
  br label %exit

bb3:
  call void @dummy()
  br label %exit

exit:
  %phi = phi i32 [ %a, %bb1 ], [ %b, %bb2 ], [ 1, %bb3]
  ret i32 %phi
}

; Don't try to merge with the entry block.
define void @entry_block() {
; CHECK-LABEL: @entry_block(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label [[LOOP:%.*]]
; CHECK:       loop:
; CHECK-NEXT:    br label [[LOOP]]
;
entry:
  br label %loop

loop:
  br label %loop
}

; For phi nodes, we need to check that incoming blocks match.
define i32 @phi_blocks(i32 %x, i32* %p) {
; CHECK-LABEL: @phi_blocks(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[SWITCH:%.*]] = icmp ult i32 [[X:%.*]], 2
; CHECK-NEXT:    br i1 [[SWITCH]], label [[EXIT:%.*]], label [[BB3:%.*]]
; CHECK:       bb3:
; CHECK-NEXT:    call void @dummy()
; CHECK-NEXT:    br label [[EXIT]]
; CHECK:       exit:
; CHECK-NEXT:    [[PHI3:%.*]] = phi i32 [ 1, [[BB3]] ], [ 0, [[ENTRY:%.*]] ]
; CHECK-NEXT:    ret i32 [[PHI3]]
;
entry:
  switch i32 %x, label %bb3 [
  i32 0, label %bb1.split
  i32 1, label %bb2
  ]

bb1.split:
  br label %bb1

bb1:
  %phi1 = phi i32 [ 0, %bb1.split ]
  br label %exit

bb2:
  %phi2 = phi i32 [ 0, %entry ]
  br label %exit

bb3:
  call void @dummy()
  br label %exit

exit:
  %phi3 = phi i32 [ %phi1, %bb1 ], [ %phi2, %bb2 ], [ 1, %bb3]
  ret i32 %phi3
}

; This requires merging bb3,4,5,6 before bb1,2.
define i32 @two_level(i32 %x, i32 %y, i32 %z) {
; CHECK-LABEL: @two_level(
; CHECK-NEXT:    switch i32 [[Z:%.*]], label [[BB7:%.*]] [
; CHECK-NEXT:    i32 0, label [[BB1:%.*]]
; CHECK-NEXT:    i32 1, label [[BB2:%.*]]
; CHECK-NEXT:    ]
; CHECK:       bb1:
; CHECK-NEXT:    [[SWITCH:%.*]] = icmp ult i32 [[X:%.*]], 2
; CHECK-NEXT:    br i1 [[SWITCH]], label [[BB3:%.*]], label [[BB7]]
; CHECK:       bb2:
; CHECK-NEXT:    [[SWITCH1:%.*]] = icmp ult i32 [[X]], 2
; CHECK-NEXT:    br i1 [[SWITCH1]], label [[BB3]], label [[BB7]]
; CHECK:       bb3:
; CHECK-NEXT:    [[A:%.*]] = add i32 [[Y:%.*]], 1
; CHECK-NEXT:    br label [[EXIT:%.*]]
; CHECK:       bb7:
; CHECK-NEXT:    call void @dummy()
; CHECK-NEXT:    br label [[EXIT]]
; CHECK:       exit:
; CHECK-NEXT:    [[PHI:%.*]] = phi i32 [ [[A]], [[BB3]] ], [ 0, [[BB7]] ]
; CHECK-NEXT:    ret i32 [[PHI]]
;
  switch i32 %z, label %bb7 [
  i32 0, label %bb1
  i32 1, label %bb2
  ]

bb1:
  switch i32 %x, label %bb7 [
  i32 0, label %bb3
  i32 1, label %bb4
  ]

bb2:
  switch i32 %x, label %bb7 [
  i32 0, label %bb5
  i32 1, label %bb6
  ]

bb3:
  %a = add i32 %y, 1
  br label %exit

bb4:
  %b = add i32 %y, 1
  br label %exit

bb5:
  %c = add i32 %y, 1
  br label %exit

bb6:
  %d = add i32 %y, 1
  br label %exit

bb7:
  call void @dummy()
  br label %exit

exit:
  %phi = phi i32 [ %a, %bb3 ], [ %b, %bb4 ], [ %c, %bb5 ], [ %d, %bb6 ], [ 0, %bb7 ]
  ret i32 %phi
}

; CHECK: !1 = !{i32 0, i32 15}
