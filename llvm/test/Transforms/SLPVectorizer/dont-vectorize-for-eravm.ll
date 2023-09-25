; RUN: opt -slp-vectorizer < %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

; Function Attrs: null_pointer_is_valid
define void @function_main() unnamed_addr {
entry:
  br i1 undef, label %bb2, label %bb3

bb1:
  %calldata_pointer384 = phi ptr addrspace(3) [ undef, %bb2 ], [ undef, %bb3 ]
  %stack_var_005.0.in = phi ptr addrspace(3) [ undef, %bb2 ], [ undef, %bb3 ]
  unreachable

bb2:                              ; preds = %entry
  br label %bb1

bb3:                 ; preds = %entry
  br label %bb1
}
