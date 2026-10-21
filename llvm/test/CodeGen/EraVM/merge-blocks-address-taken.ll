; RUN: opt -S -mtriple=eravm -passes=mergebb < %s | FileCheck %s

@gv = global ptr null

declare void @sink(i32)

; The address of %b is taken, so it must not be merged into %a and erased.
define void @dropped_block_address_taken(i32 %x) {
; CHECK-LABEL: @dropped_block_address_taken(
; CHECK:         store ptr blockaddress(@dropped_block_address_taken, %b), ptr @gv
; CHECK:       a:
; CHECK-NEXT:    call void @sink(i32 1)
; CHECK:       b:
; CHECK-NEXT:    call void @sink(i32 1)
;
entry:
  store ptr blockaddress(@dropped_block_address_taken, %b), ptr @gv
  switch i32 %x, label %d [
    i32 0, label %a
    i32 1, label %b
  ]

a:
  call void @sink(i32 1)
  br label %exit

b:
  call void @sink(i32 1)
  br label %exit

d:
  ret void

exit:
  ret void
}

; The address of the kept block %a is taken. %b can still be merged into it.
define void @kept_block_address_taken(i32 %x) {
; CHECK-LABEL: @kept_block_address_taken(
; CHECK:         store ptr blockaddress(@kept_block_address_taken, %a), ptr @gv
; CHECK:       a:
; CHECK-NEXT:    call void @sink(i32 1)
; CHECK-NOT:   call void @sink(i32 1)
;
entry:
  store ptr blockaddress(@kept_block_address_taken, %a), ptr @gv
  switch i32 %x, label %d [
    i32 0, label %a
    i32 1, label %b
  ]

a:
  call void @sink(i32 1)
  br label %exit

b:
  call void @sink(i32 1)
  br label %exit

d:
  ret void

exit:
  ret void
}
