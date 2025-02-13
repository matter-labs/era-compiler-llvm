; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --function-signature --check-attributes
; RUN: opt < %s -aa-pipeline=basic-aa -passes='cgscc(function-attrs),rpo-function-attrs' -S | FileCheck %s
; XFAIL: target=eravm{{.*}}, target=evm{{.*}}

define i32 @leaf() {
; CHECK: Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none)
; CHECK-LABEL: define {{[^@]+}}@leaf
; CHECK-SAME: () #[[ATTR0:[0-9]+]] {
; CHECK-NEXT:    ret i32 1
;
  ret i32 1
}

define i32 @self_rec() {
; CHECK: Function Attrs: nofree nosync nounwind memory(none)
; CHECK-LABEL: define {{[^@]+}}@self_rec
; CHECK-SAME: () #[[ATTR1:[0-9]+]] {
; CHECK-NEXT:    [[A:%.*]] = call i32 @self_rec()
; CHECK-NEXT:    ret i32 4
;
  %a = call i32 @self_rec()
  ret i32 4
}

define i32 @indirect_rec() {
; CHECK: Function Attrs: nofree nosync nounwind memory(none)
; CHECK-LABEL: define {{[^@]+}}@indirect_rec
; CHECK-SAME: () #[[ATTR1]] {
; CHECK-NEXT:    [[A:%.*]] = call i32 @indirect_rec2()
; CHECK-NEXT:    ret i32 [[A]]
;
  %a = call i32 @indirect_rec2()
  ret i32 %a
}

define i32 @indirect_rec2() {
; CHECK: Function Attrs: nofree nosync nounwind memory(none)
; CHECK-LABEL: define {{[^@]+}}@indirect_rec2
; CHECK-SAME: () #[[ATTR1]] {
; CHECK-NEXT:    [[A:%.*]] = call i32 @indirect_rec()
; CHECK-NEXT:    ret i32 [[A]]
;
  %a = call i32 @indirect_rec()
  ret i32 %a
}

define i32 @extern() {
; CHECK: Function Attrs: nofree nosync memory(none)
; CHECK-LABEL: define {{[^@]+}}@extern
; CHECK-SAME: () #[[ATTR2:[0-9]+]] {
; CHECK-NEXT:    [[A:%.*]] = call i32 @k()
; CHECK-NEXT:    ret i32 [[A]]
;
  %a = call i32 @k()
  ret i32 %a
}

declare i32 @k() readnone

define void @intrinsic(ptr %dest, ptr %src, i32 %len) {
; CHECK: Function Attrs: mustprogress nofree nosync nounwind willreturn memory(argmem: readwrite)
; CHECK-LABEL: define {{[^@]+}}@intrinsic
; CHECK-SAME: (ptr nocapture writeonly [[DEST:%.*]], ptr nocapture readonly [[SRC:%.*]], i32 [[LEN:%.*]]) #[[ATTR4:[0-9]+]] {
; CHECK-NEXT:    call void @llvm.memcpy.p0.p0.i32(ptr [[DEST]], ptr [[SRC]], i32 [[LEN]], i1 false)
; CHECK-NEXT:    ret void
;
  call void @llvm.memcpy.p0.p0.i32(ptr %dest, ptr %src, i32 %len, i1 false)
  ret void
}

declare void @llvm.memcpy.p0.p0.i32(ptr, ptr, i32, i1)

define internal i32 @called_by_norecurse() {
; CHECK: Function Attrs: nofree norecurse nosync memory(none)
; CHECK-LABEL: define {{[^@]+}}@called_by_norecurse
; CHECK-SAME: () #[[ATTR6:[0-9]+]] {
; CHECK-NEXT:    [[A:%.*]] = call i32 @k()
; CHECK-NEXT:    ret i32 [[A]]
;
  %a = call i32 @k()
  ret i32 %a
}

define void @m() norecurse {
; CHECK: Function Attrs: nofree norecurse nosync memory(none)
; CHECK-LABEL: define {{[^@]+}}@m
; CHECK-SAME: () #[[ATTR6]] {
; CHECK-NEXT:    [[A:%.*]] = call i32 @called_by_norecurse()
; CHECK-NEXT:    ret void
;
  %a = call i32 @called_by_norecurse()
  ret void
}

define internal i32 @called_by_norecurse_indirectly() {
; CHECK: Function Attrs: nofree norecurse nosync memory(none)
; CHECK-LABEL: define {{[^@]+}}@called_by_norecurse_indirectly
; CHECK-SAME: () #[[ATTR6]] {
; CHECK-NEXT:    [[A:%.*]] = call i32 @k()
; CHECK-NEXT:    ret i32 [[A]]
;
  %a = call i32 @k()
  ret i32 %a
}

define internal void @o() {
; CHECK: Function Attrs: nofree norecurse nosync memory(none)
; CHECK-LABEL: define {{[^@]+}}@o
; CHECK-SAME: () #[[ATTR6]] {
; CHECK-NEXT:    [[A:%.*]] = call i32 @called_by_norecurse_indirectly()
; CHECK-NEXT:    ret void
;
  %a = call i32 @called_by_norecurse_indirectly()
  ret void
}

define void @p() norecurse {
; CHECK: Function Attrs: nofree norecurse nosync memory(none)
; CHECK-LABEL: define {{[^@]+}}@p
; CHECK-SAME: () #[[ATTR6]] {
; CHECK-NEXT:    call void @o()
; CHECK-NEXT:    ret void
;
  call void @o()
  ret void
}

define internal i32 @escapes_as_parameter(ptr %p) {
; CHECK: Function Attrs: nofree nosync memory(none)
; CHECK-LABEL: define {{[^@]+}}@escapes_as_parameter
; CHECK-SAME: (ptr nocapture readnone [[P:%.*]]) #[[ATTR2]] {
; CHECK-NEXT:    [[A:%.*]] = call i32 @k()
; CHECK-NEXT:    ret i32 [[A]]
;
  %a = call i32 @k()
  ret i32 %a
}

define internal void @q() {
; CHECK: Function Attrs: nofree norecurse nosync memory(none)
; CHECK-LABEL: define {{[^@]+}}@q
; CHECK-SAME: () #[[ATTR6]] {
; CHECK-NEXT:    [[A:%.*]] = call i32 @escapes_as_parameter(ptr @escapes_as_parameter)
; CHECK-NEXT:    ret void
;
  %a = call i32 @escapes_as_parameter(ptr @escapes_as_parameter)
  ret void
}

define void @r() norecurse {
; CHECK: Function Attrs: nofree norecurse nosync memory(none)
; CHECK-LABEL: define {{[^@]+}}@r
; CHECK-SAME: () #[[ATTR6]] {
; CHECK-NEXT:    call void @q()
; CHECK-NEXT:    ret void
;
  call void @q()
  ret void
}
