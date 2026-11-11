; RUN: opt -passes=instcombine -S < %s | FileCheck %s

; Check that merging two GEPs doesn't keep the nuw flag of the second GEP.
; The merged offset is -1, so a nuw GEP would be poison for any %p != 0.

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define ptr addrspace(1) @gep_merge_not_nuw(ptr addrspace(1) %p, i256 %idx) {
; CHECK-LABEL: @gep_merge_not_nuw(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[P:%.*]], i256 -1
; CHECK-NEXT:    ret ptr addrspace(1) [[GEP]]
;
  %idx.neg = sub i256 0, %idx
  %add = add i256 %idx, -1
  %gep1 = getelementptr inbounds i8, ptr addrspace(1) %p, i256 %idx.neg
  %gep = getelementptr inbounds nuw i8, ptr addrspace(1) %gep1, i256 %add
  ret ptr addrspace(1) %gep
}
