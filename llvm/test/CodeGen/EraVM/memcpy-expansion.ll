; RUN: llc -opaque-pointers -stop-after eravm-alloca-hoisting < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

; Function Attrs: argmemonly mustprogress nofree nounwind willreturn
declare void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(3) noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p1.p1.i256(ptr addrspace(1) noalias nocapture writeonly, ptr addrspace(1) noalias nocapture readonly, i256, i1 immarg)

; CHECK-LABEL: expand-known
define fastcc void @expand-known(ptr addrspace(1) %dest, ptr addrspace(3) %src) {
; Loop
; CHECK:   %loop-index = phi i256 [ 0, %{{.*}} ], [ [[NEWCTR:%[0-9]+]], %load-store-loop ]
; CHECK:   [[REG:%[0-9]+]] = load i256, ptr addrspace(3) %{{[0-9]+}}, align 1
; CHECK:   store i256 [[REG]], ptr addrspace(1) %{{[0-9]+}}, align 1
; CHECK:   [[NEWCTR]] = add i256 %loop-index, 1

; Residual
; CHECK:   [[PART1:%[0-9]+]] = load i256, ptr addrspace(3) %{{[0-9]+}}, align 1
; CHECK:   [[PART1_M:%[0-9]+]] = and i256 [[PART1]], -{{[0-9]+}}
; CHECK:   [[PART2:%[0-9]+]] = load i256, ptr addrspace(1) %{{[0-9]+}}, align 1
; CHECK:   [[PART2_M:%[0-9]+]] = and i256 [[PART2]], {{[0-9]+}}
; CHECK:   [[RES:%[0-9]+]] = or i256 [[PART1_M]], [[PART2_M]]
; CHECK:   store i256 [[RES]], ptr addrspace(1) %{{[0-9]+}}, align 1
  call void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) %dest, ptr addrspace(3) %src, i256 42, i1 false)
  ret void
}

; CHECK-LABEL: expand-unknown
define fastcc void @expand-unknown(ptr addrspace(1) %dest, ptr addrspace(3) %src, i256 %size) {
; CHECK:   %loop-count = udiv i256 %size, 32
; CHECK:   %residual-bytes = urem i256 %size, 32
; CHECK:   [[COND1:%[0-9]+]] = icmp ne i256 %loop-count, 0
; CHECK:   br i1 [[COND1]], label %{{.*}}, label %{{.*}}

; Loop
; CHECK:   [[REG:%[0-9]+]] = load i256, ptr addrspace(3) %{{[0-9]+}}, align 1
; CHECK:   store i256 [[REG]], ptr addrspace(1) %{{[0-9]+}}, align 1
; CHECK:   br i1 %{{[0-9]+}}, label %{{.*}}, label %{{.*}}

; Residual
; CHECK:   %{{[0-9]+}} = load i256, ptr addrspace(3) %{{[0-9]+}}, align 1
; CHECK:   %{{[0-9]+}} = load i256, ptr addrspace(1) %{{[0-9]+}}, align 1
; CHECK:   [[RES:%[0-9]+]] = or i256 %{{[0-9]+}}, %{{[0-9]+}}
; CHECK:   store i256 [[RES]], ptr addrspace(1) %{{[0-9]+}}, align 1

; Residual condition:
; CHECK:   [[COND2:%[0-9]+]] = icmp ne i256 %residual-bytes, 0
; CHECK:   br i1 [[COND2]], label %{{.*}}, label %{{.*}}
  call void @llvm.memcpy.p1.p3.i256(ptr addrspace(1) %dest, ptr addrspace(3) %src, i256 %size, i1 false)
  ret void
}
