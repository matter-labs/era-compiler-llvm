; RUN: llc --disable-eravm-scalar-opt-passes -stop-before verify < %s | FileCheck %s
; RUN: llc -O3 < %s | FileCheck --check-prefix=CHECK-INSTRS %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

; Function Attrs: argmemonly mustprogress nofree nounwind willreturn
declare void @llvm.memcpy.p1i256.p3i256.i256(i256 addrspace(1)* noalias nocapture writeonly, i256 addrspace(3)* noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p1i256.p1i256.i256(i256 addrspace(1)* noalias nocapture writeonly, i256 addrspace(1)* noalias nocapture readonly, i256, i1 immarg)

; CHECK-LABEL: expand-known
define fastcc void @expand-known(i256 addrspace(1)* %dest, i256 addrspace(3)* %src) {
; CHECK:   br label %load-store-loop
; CHECK: load-store-loop:
; CHECK:   %loop-index = phi i256 [ 0, %{{.*}} ], [ [[NEWCTR:%[0-9]+]], %load-store-loop ]
; CHECK:   [[REG:%[0-9]+]] = load i256, ptr addrspace(3) %{{[0-9]+}}, align 1
; CHECK:   store i256 [[REG]], ptr addrspace(1) %{{[0-9]+}}, align 1
; CHECK:   [[NEWCTR]] = add i256 %loop-index, 1
; CHECK:   br i1 %{{[0-9]+}}, label %load-store-loop, label %memcpy-split
; CHECK: memcpy-split:
; CHECK:   [[PART1:%[0-9]+]] = load i256, ptr addrspace(3) %{{[0-9]+}}, align 1
; CHECK:   [[PART1_M:%[0-9]+]] = and i256 [[PART1]], -{{[0-9]+}}
; CHECK:   [[PART2:%[0-9]+]] = load i256, ptr addrspace(1) %{{[0-9]+}}, align 1
; CHECK:   [[PART2_M:%[0-9]+]] = and i256 [[PART2]], {{[0-9]+}}
; CHECK:   [[RES:%[0-9]+]] = or i256 [[PART1_M]], [[PART2_M]]
; CHECK:   store i256 [[RES]], ptr addrspace(1) %{{[0-9]+}}, align 1
  call void @llvm.memcpy.p1i256.p3i256.i256(i256 addrspace(1)* %dest, i256 addrspace(3)* %src, i256 42, i1 false)
  ret void
}

; CHECK-LABEL: expand-unknown
define fastcc void @expand-unknown(i256 addrspace(1)* %dest, i256 addrspace(3)* %src, i256 %size) {
; CHECK:   %loop-count = udiv i256 %size, 32
; CHECK:   %residual-bytes = urem i256 %size, 32
; CHECK:   [[COND1:%[0-9]+]] = icmp ne i256 %loop-count, 0
; CHECK:   br i1 [[COND1]], label %load-store-loop-preheader, label %memcpy-residual-cond
; CHECK: load-store-loop-preheader:
; CHECK:   br label %load-store-loop
; CHECK: load-store-loop:
; CHECK:   %loop-index = phi i256 [ 0, %{{.*}} ], [ [[NEWCTR:%[0-9]+]], %load-store-loop ]
; CHECK:   [[REG:%[0-9]+]] = load i256, ptr addrspace(3) %{{[0-9]+}}, align 1
; CHECK:   store i256 [[REG]], ptr addrspace(1) %{{[0-9]+}}, align 1
; CHECK:   [[NEWCTR]] = add i256 %loop-index, 1
; CHECK:   br i1 %{{[0-9]+}}, label %load-store-loop, label %load-store-loop-exit
; CHECK: load-store-loop-exit:
; CHECK:   br label %memcpy-residual-cond
; CHECK: memcpy-residual-cond:
; CHECK:   [[COND2:%[0-9]+]] = icmp ne i256 %residual-bytes, 0
; CHECK:   br i1 [[COND2]], label %memcpy-residual, label %memcpy-split
; CHECK: memcpy-residual:
; CHECK:   %{{[0-9]+}} = load i256, ptr addrspace(3) %{{[0-9]+}}, align 1
; CHECK:   %{{[0-9]+}} = load i256, ptr addrspace(1) %{{[0-9]+}}, align 1
; CHECK:   [[RES:%[0-9]+]] = or i256 %{{[0-9]+}}, %{{[0-9]+}}
; CHECK:   store i256 [[RES]], ptr addrspace(1) %{{[0-9]+}}, align 1
; CHECK:   br label %memcpy-split
  call void @llvm.memcpy.p1i256.p3i256.i256(i256 addrspace(1)* %dest, i256 addrspace(3)* %src, i256 %size, i1 false)
  ret void
}

; CHECK-INSTRS-LABEL: expand-unknown-instrs
define fastcc void @expand-unknown-instrs(i256 addrspace(1)* %dest, i256 addrspace(1)* %src, i256 %size) {
; Preheader and loop.
; CHECK-INSTRS:       add r1, r3, r5
; CHECK-INSTRS-NEXT:  add r2, r0, r6
; CHECK-INSTRS-NEXT:  add r1, r0, r7
; CHECK-INSTRS-NEXT:  .BB2_2:
; CHECK-INSTRS:       ldmi.h r6, r8, r6
; CHECK-INSTRS-NEXT:  stmi.h r7, r8, r7
; CHECK-INSTRS-NEXT:  sub! r7, r5, r8
; CHECK-INSTRS-NEXT:  jump.ne @.BB2_2
  call void @llvm.memcpy.p1i256.p1i256.i256(i256 addrspace(1)* %dest, i256 addrspace(1)* %src, i256 %size, i1 false)
  ret void
}
