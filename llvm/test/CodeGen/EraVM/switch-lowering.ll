; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

; CHECK-LABEL: __runtime
define void @__runtime() minsize {
entry:
  switch i32 undef, label %if_join [
    i32 1334695181, label %switch_case_branch_1_block
    i32 2137741580, label %switch_case_branch_2_block
    i32 -849866765, label %switch_case_branch_3_block
    i32 -343226079, label %switch_case_branch_4_block
    i32 -251791308, label %switch_case_branch_5_block563
    i32 -209920882, label %switch_case_branch_6_block
  ]

if_join:                                          ; preds = %entry
  unreachable

switch_case_branch_1_block:                       ; preds = %entry
  unreachable

switch_case_branch_2_block:                       ; preds = %entry
  unreachable

switch_case_branch_3_block:                       ; preds = %entry
  unreachable

switch_case_branch_4_block:                       ; preds = %entry
  br i1 undef, label %if_main204, label %if_join205

if_main204:                                       ; preds = %switch_case_branch_4_block
  unreachable

; CHECK-NOT: JTI
; TODO: CPR-688 It might worth making a jumptable here, but
;       EraVM doesn't support it now.
if_join205:                                       ; preds = %switch_case_branch_4_block
  switch i256 undef, label %switch_default_block [
    i256 1, label %switch_case_branch_1_block418
    i256 2, label %switch_case_branch_2_block442
    i256 3, label %switch_case_branch_3_block466
    i256 4, label %switch_case_branch_4_block491
    i256 5, label %switch_case_branch_5_block
  ]

switch_case_branch_1_block418:                    ; preds = %if_join205
  unreachable

switch_case_branch_2_block442:                    ; preds = %if_join205
  unreachable

switch_case_branch_3_block466:                    ; preds = %if_join205
  unreachable

switch_case_branch_4_block491:                    ; preds = %if_join205
  unreachable

switch_case_branch_5_block:                       ; preds = %if_join205
  unreachable

switch_default_block:                             ; preds = %if_join205
  unreachable

switch_case_branch_5_block563:                    ; preds = %entry
  unreachable

switch_case_branch_6_block:                       ; preds = %entry
  unreachable
}
