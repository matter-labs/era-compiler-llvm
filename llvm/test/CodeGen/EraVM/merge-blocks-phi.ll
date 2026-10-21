; RUN: opt -S -mtriple=eravm -passes=mergebb < %s | FileCheck %s

declare void @sink(i32)

; The blocks %a and %b have PHIs, so merging %b into %a would leave the PHI of
; %a without an incoming value for %p2.
define void @unused_phi(i32 %x) {
; CHECK-LABEL: @unused_phi(
; CHECK:       a:
; CHECK-NEXT:    [[PA:%.*]] = phi i32 [ 1, [[P1:%.*]] ]
; CHECK-NEXT:    call void @sink(i32 7)
; CHECK:       b:
; CHECK-NEXT:    [[PB:%.*]] = phi i32 [ 1, [[P2:%.*]] ]
; CHECK-NEXT:    call void @sink(i32 7)
;
entry:
  switch i32 %x, label %d [
    i32 0, label %p1
    i32 1, label %p2
  ]

p1:
  br label %a

p2:
  br label %b

a:
  %pa = phi i32 [ 1, %p1 ]
  call void @sink(i32 7)
  br label %exit

b:
  %pb = phi i32 [ 1, %p2 ]
  call void @sink(i32 7)
  br label %exit

d:
  ret void

exit:
  ret void
}

define void @used_phi(i32 %x) {
; CHECK-LABEL: @used_phi(
; CHECK:       a:
; CHECK-NEXT:    [[PA:%.*]] = phi i32 [ 1, [[P1:%.*]] ]
; CHECK-NEXT:    call void @sink(i32 [[PA]])
; CHECK:       b:
; CHECK-NEXT:    [[PB:%.*]] = phi i32 [ 1, [[P2:%.*]] ]
; CHECK-NEXT:    call void @sink(i32 [[PB]])
;
entry:
  switch i32 %x, label %d [
    i32 0, label %p1
    i32 1, label %p2
  ]

p1:
  br label %a

p2:
  br label %b

a:
  %pa = phi i32 [ 1, %p1 ]
  call void @sink(i32 %pa)
  br label %exit

b:
  %pb = phi i32 [ 1, %p2 ]
  call void @sink(i32 %pb)
  br label %exit

d:
  ret void

exit:
  ret void
}
