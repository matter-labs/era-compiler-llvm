; RUN: llc < %s

; EVMSingleUseExpression rematerializes CALLDATALOAD when loading from a
; constant address.
; Before the fix, this broke when CALLDATALOAD redefined its input virtual
; register.
; Reduced from issue #136; minimized with bugpoint.
; https://github.com/matter-labs/solx/issues/136
; Triggers the coalescer to coalesce the CALLDATALOAD def and its use.
target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

define void @__entry() {
entry:
  switch i32 poison, label %if_join [
    i32 306754293, label %switch_case_branch_1_block
    i32 1826699332, label %switch_case_branch_2_block
    i32 -974522369, label %switch_case_branch_3_block
    i32 -171475469, label %switch_case_branch_4_block
  ]

if_join:                                          ; preds = %entry
  unreachable

switch_case_branch_1_block:                       ; preds = %entry
  unreachable

switch_case_branch_2_block:                       ; preds = %entry
  unreachable

switch_case_branch_3_block:                       ; preds = %entry
  %calldata_load_result.i = load i256, ptr addrspace(2) inttoptr (i256 4 to ptr addrspace(2)), align 4
  %calldata_load_result.i51 = load i256, ptr addrspace(2) inttoptr (i256 68 to ptr addrspace(2)), align 4
  br i1 poison, label %switch_join_block.i, label %checked_div_t_int256.exit.i

switch_join_block.i:                              ; preds = %switch_case_branch_3_block
  %comparison_result6.i.i = icmp eq i256 %calldata_load_result.i51, -1
  %comparison_result9.i.i = icmp eq i256 %calldata_load_result.i, -57896044618658097711785492504343953926634992332820282019728792003956564819968
  %and_result21.i.i = and i1 %comparison_result9.i.i, %comparison_result6.i.i
  br i1 %and_result21.i.i, label %if_main12.i.i, label %checked_div_t_int256.exit.i

if_main12.i.i:                                    ; preds = %switch_join_block.i
  unreachable

checked_div_t_int256.exit.i:                      ; preds = %switch_join_block.i, %switch_case_branch_3_block
  %expr_38.0.i7078 = phi i256 [ %calldata_load_result.i51, %switch_join_block.i ], [ 68, %switch_case_branch_3_block ]
  %division_signed_result_non_zero.i.i = sdiv i256 %calldata_load_result.i, %expr_38.0.i7078
  %remainder_signed_result_non_zero.i.i = srem i256 %division_signed_result_non_zero.i.i, %calldata_load_result.i
  store i256 %remainder_signed_result_non_zero.i.i, ptr addrspace(1) inttoptr (i256 128 to ptr addrspace(1)), align 128
  unreachable

switch_case_branch_4_block:                       ; preds = %entry
  unreachable
}
