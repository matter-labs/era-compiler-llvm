; RUN: llc < %s | FileCheck %s

target datalayout = "e-p:16:8-i256:256:256"
target triple = "syncvm"

; CHECK-LABEL: store_to_frame
define void @store_to_frame(i256 %par) nounwind {
  %1 = alloca i256
; CHECK: mst	r1, 0(sp)
  store i256 %par, i256* %1
  ret void
}

; CHECK-LABEL: store_to_frame2
define void @store_to_frame2(i256 %par) nounwind {
  %1 = alloca i256
  %2 = alloca i256
; CHECK: mst	r1, 0(sp)
  store i256 %par, i256* %1
; CHECK: mst	r1, 32(sp)
  store i256 %par, i256* %2
  ret void
}

; CHECK-LABEL: load_from_frame
define i256 @load_from_frame(i256 %par) nounwind {
  %1 = alloca i256
; CHECK: mst	r1, 0(sp)
  store i256 %par, i256* %1
  %2 = call i256 @foo()
; CHECK: mld	0(sp), r1
  %3 = load i256, i256* %1
  ret i256 %3
}

declare i256 @foo()
