; RUN: llc -stop-before verify < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm-unknown-unknown"

; Function Attrs: argmemonly mustprogress nofree nounwind willreturn
declare void @llvm.memcpy.p1i256.p3i256.i256(i256 addrspace(1)* noalias nocapture writeonly, i256 addrspace(3)* noalias nocapture readonly, i256, i1 immarg)

; CHECK-LABEL: expand-known
define fastcc void @expand-known(i256 addrspace(1)* %dest, i256 addrspace(3)* %src) {
; CHECK:   br label %load-store-loop
; CHECK: load-store-loop:
; CHECK:   %loop-index = phi i256 [ 0, %{{.*}} ], [ [[NEWCTR:%[0-9]+]], %load-store-loop ]
; CHECK:   [[REG:%[0-9]+]] = load i256, i256 addrspace(3)* %{{[0-9]+}}, align 1
; CHECK:   store i256 [[REG]], i256 addrspace(1)* %{{[0-9]+}}, align 1
; CHECK:   [[NEWCTR]] = add i256 %loop-index, 1
; CHECK:   br i1 %{{[0-9]+}}, label %load-store-loop, label %memcpy-split
; CHECK: memcpy-split:
; CHECK:   [[PART1:%[0-9]+]] = load i256, i256 addrspace(3)* %{{[0-9]+}}, align 1
; CHECK:   [[PART1_M:%[0-9]+]] = and i256 [[PART1]], -{{[0-9]+}}
; CHECK:   [[PART2:%[0-9]+]] = load i256, i256 addrspace(1)* %{{[0-9]+}}, align 1
; CHECK:   [[PART2_M:%[0-9]+]] = and i256 [[PART2]], {{[0-9]+}}
; CHECK:   [[RES:%[0-9]+]] = or i256 [[PART1_M]], [[PART2_M]]
; CHECK:   store i256 [[RES]], i256 addrspace(1)* %{{[0-9]+}}, align 1
  call void @llvm.memcpy.p1i256.p3i256.i256(i256 addrspace(1)* %dest, i256 addrspace(3)* %src, i256 42, i1 false)
  ret void
}

; CHECK-LABEL: expand-unknown
define fastcc void @expand-unknown(i256 addrspace(1)* %dest, i256 addrspace(3)* %src, i256 %size) {
; CHECK:   %loop-count = udiv i256 %size, 32
; CHECK:   %residual-bytes = urem i256 %size, 32
; CHECK:   [[COND1:%[0-9]+]] = icmp ne i256 %loop-count, 0
; CHECK:   br i1 [[COND1]], label %load-store-loop, label %memcpy-residual-cond
; CHECK: load-store-loop:
; CHECK:   %loop-index = phi i256 [ 0, %{{.*}} ], [ [[NEWCTR:%[0-9]+]], %load-store-loop ]
; CHECK:   [[REG:%[0-9]+]] = load i256, i256 addrspace(3)* %{{[0-9]+}}, align 1
; CHECK:   store i256 [[REG]], i256 addrspace(1)* %{{[0-9]+}}, align 1
; CHECK:   [[NEWCTR]] = add i256 %loop-index, 1
; CHECK:   br i1 %{{[0-9]+}}, label %load-store-loop, label %memcpy-residual-cond
; CHECK: memcpy-residual:
; CHECK:   %{{[0-9]+}} = load i256, i256 addrspace(3)* %{{[0-9]+}}, align 1
; CHECK:   %{{[0-9]+}} = load i256, i256 addrspace(1)* %{{[0-9]+}}, align 1
; CHECK:   [[RES:%[0-9]+]] = or i256 %{{[0-9]+}}, %{{[0-9]+}}
; CHECK:   store i256 [[RES]], i256 addrspace(1)* %{{[0-9]+}}, align 1
; CHECK:   br label %memcpy-split
; CHECK: memcpy-residual-cond:
; CHECK:   [[COND2:%[0-9]+]] = icmp ne i256 %residual-bytes, 0
; CHECK:   br i1 [[COND2]], label %memcpy-residual, label %memcpy-split
  call void @llvm.memcpy.p1i256.p3i256.i256(i256 addrspace(1)* %dest, i256 addrspace(3)* %src, i256 %size, i1 false)
  ret void
}
