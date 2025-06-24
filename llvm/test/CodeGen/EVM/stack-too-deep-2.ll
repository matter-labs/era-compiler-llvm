; REQUIRES: asserts
; RUN: llc -evm-stack-region-offset=128 -evm-stack-region-size=192 --debug-only=evm-stack-solver < %s 2>&1 | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm-unknown-unknown"

declare i256 @checked_mul_uint8(i256)

; Check that the stack solver detects unreachable slots, generates spills for them, and
; succesfully compiles the function. Also, check that we allocated the exact amount of
; stack space needed for the function, without any warnings about allocated stack region size.

; CHECK: Unreachable slots found: 30, iteration: 1
; CHECK: Spilling 2 registers
; CHECK: Unreachable slots found: 20, iteration: 2
; CHECK: Spilling 1 registers
; CHECK: Unreachable slots found: 8, iteration: 3
; CHECK: Spilling 1 registers
; CHECK: Unreachable slots found: 6, iteration: 4
; CHECK: Spilling 1 registers
; CHECK: Unreachable slots found: 2, iteration: 5
; CHECK: Spilling 1 registers
; CHECK-NOT: warning: allocated stack region size:

define fastcc i256 @fun_test_462(i256 %0) unnamed_addr {
entry:
  %and_result3 = and i256 %0, 255
  %trunc = trunc i256 %0 to i8
  %comparison_result38 = icmp eq i256 %and_result3, 22
  %comparison_result45 = icmp eq i256 %and_result3, 33
  %comparison_result68 = icmp eq i256 %and_result3, 3
  %comparison_result113 = icmp eq i256 %and_result3, 4
  %comparison_result126 = icmp eq i256 %and_result3, 24
  %comparison_result187 = icmp eq i256 %and_result3, 26
  %comparison_result237 = icmp eq i256 %and_result3, 31
  %comparison_result255 = icmp eq i256 %and_result3, 32
  %comparison_result282 = icmp eq i256 %and_result3, 28
  %comparison_result330 = icmp eq i256 %and_result3, 6
  br label %for_condition

return.loopexit572.split.loop.exit599:            ; preds = %if_join42
  %var_cnt.2.mux.le = select i1 %spec.select, i256 %var_cnt.2, i256 30
  br label %return

return:                                           ; preds = %if_join295, %for_join195, %for_body193, %for_body132, %if_join110, %checked_mul_uint8_1420.exit, %for_body, %for_condition, %return.loopexit572.split.loop.exit599
  %return_pointer.0 = phi i256 [ 10, %checked_mul_uint8_1420.exit ], [ %var_cnt.2.mux.le, %return.loopexit572.split.loop.exit599 ], [ 80, %if_join295 ], [ 70, %for_body193 ], [ 60, %for_join195 ], [ 50, %for_body132 ], [ 40, %if_join110 ], [ 0, %for_body ], [ %var_cnt.0, %for_condition ]
  ret i256 %return_pointer.0

for_condition:                                    ; preds = %for_increment, %entry
  %var_i.0 = phi i256 [ 0, %entry ], [ %addition_result348, %for_increment ]
  %var_cnt.0 = phi i256 [ 0, %entry ], [ %var_cnt.1, %for_increment ]
  %comparison_result = icmp ult i256 %var_i.0, 2
  br i1 %comparison_result, label %for_body, label %return

for_body:                                         ; preds = %for_condition
  switch i8 %trunc, label %for_condition22 [
    i8 1, label %checked_mul_uint8_1420.exit
    i8 21, label %return
    i8 11, label %for_increment
  ]

for_increment:                                    ; preds = %for_condition22, %for_body
  %var_cnt.1 = phi i256 [ %var_cnt.0, %for_body ], [ %var_cnt.2, %for_condition22 ]
  %addition_result348 = add nuw nsw i256 %var_i.0, 1
  br label %for_condition

checked_mul_uint8_1420.exit:                      ; preds = %for_body
  br label %return

for_condition22:                                  ; preds = %for_join65, %for_body
  %var_j.0 = phi i256 [ %checked_mul_uint8_call, %for_join65 ], [ 1, %for_body ]
  %var_cnt.2 = phi i256 [ %var_cnt.3.lcssa541, %for_join65 ], [ %var_cnt.0, %for_body ]
  %comparison_result29 = icmp ugt i256 %var_j.0, 3
  %or.cond = or i1 %comparison_result38, %comparison_result29
  br i1 %or.cond, label %for_increment, label %if_join42

if_join42:                                        ; preds = %for_condition22
  %comparison_result55 = icmp ugt i256 %var_j.0, 1
  %spec.select = and i1 %comparison_result45, %comparison_result55
  %brmerge = or i1 %spec.select, %comparison_result68
  br i1 %brmerge, label %return.loopexit572.split.loop.exit599, label %for_condition62.outer

for_condition62.outer:                            ; preds = %if_join117, %if_join42
  %var_p.0.ph = phi i256 [ 0, %if_join42 ], [ %addition_result.i, %if_join117 ]
  %var_cnt.3.ph = phi i256 [ %var_cnt.2, %if_join42 ], [ %var_cnt.5, %if_join117 ]
  br label %for_condition62

for_condition62:                                  ; preds = %if_join84, %for_condition62.outer
  %var_p.0 = phi i256 [ %addition_result.i, %if_join84 ], [ %var_p.0.ph, %for_condition62.outer ]
  %and_result.i410 = and i256 %var_p.0, 255
  %comparison_result.i = icmp ugt i256 %and_result.i410, 253
  br i1 %comparison_result.i, label %shift_left_non_overflow.i411, label %checked_add_uint8.exit

for_join65:                                       ; preds = %if_join84, %checked_add_uint8.exit
  %var_cnt.3.lcssa541 = phi i256 [ %var_cnt.2, %if_join84 ], [ %var_cnt.3.ph, %checked_add_uint8.exit ]
  %checked_mul_uint8_call = tail call fastcc i256 @checked_mul_uint8(i256 %var_j.0)
  br label %for_condition22

shift_left_non_overflow.i411:                     ; preds = %if_join309, %for_condition62
  unreachable

checked_add_uint8.exit:                           ; preds = %for_condition62
  %addition_result.i = add nuw nsw i256 %and_result.i410, 2
  %and_result78 = and i256 %addition_result.i, 7
  %comparison_result80 = icmp eq i256 %and_result78, 0
  br i1 %comparison_result80, label %for_join65, label %if_join84

if_join84:                                        ; preds = %checked_add_uint8.exit
  %trunc2 = trunc i256 %and_result78 to i8
  switch i8 %trunc2, label %if_join110 [
    i8 12, label %for_condition62
    i8 23, label %for_join65
  ]

if_join110:                                       ; preds = %increment_uint8.exit, %if_join84
  %var_h.0 = phi i256 [ %addition_result.i414, %increment_uint8.exit ], [ 1, %if_join84 ]
  %var_cnt.5 = phi i256 [ %var_cnt.6.lcssa, %increment_uint8.exit ], [ %var_cnt.3.ph, %if_join84 ]
  br i1 %comparison_result113, label %return, label %if_join117

if_join117:                                       ; preds = %if_join110
  %comparison_result119 = icmp ugt i256 %var_h.0, 2
  %or.cond401 = or i1 %comparison_result126, %comparison_result119
  br i1 %or.cond401, label %for_condition62.outer, label %for_condition131

for_condition131:                                 ; preds = %for_join159, %if_join117
  %var_k.0 = phi i256 [ %addition_result336, %for_join159 ], [ 10, %if_join117 ]
  %var_cnt.6 = phi i256 [ %var_cnt.7.ph645, %for_join159 ], [ %var_cnt.5, %if_join117 ]
  %comparison_result137 = icmp ult i256 %var_k.0, 12
  br i1 %comparison_result137, label %for_body132, label %increment_uint8.exit

for_body132:                                      ; preds = %for_condition131
  %trunc3 = trunc i256 %var_cnt.6 to i8
  switch i8 %trunc3, label %if_join167.outer [
    i8 5, label %return
    i8 25, label %increment_uint8.exit
  ]

increment_uint8.exit:                             ; preds = %for_body132, %for_condition131
  %var_cnt.6.lcssa = phi i256 [ %var_cnt.6, %for_condition131 ], [ %var_cnt.5, %for_body132 ]
  %addition_result.i414 = add nuw nsw i256 %var_h.0, 1
  br label %if_join110

for_join159:                                      ; preds = %if_join184, %if_join167
  %addition_result336 = add nuw nsw i256 %var_k.0, 1
  br label %for_condition131

if_join167.outer:                                 ; preds = %if_join167.outer.backedge, %for_body132
  %var_x.0.ph = phi i256 [ %addition_result, %if_join167.outer.backedge ], [ 7, %for_body132 ]
  %var_cnt.7.ph645 = phi i256 [ %var_cnt.7.ph645.be, %if_join167.outer.backedge ], [ %var_cnt.6, %for_body132 ]
  br label %if_join167

if_join167:                                       ; preds = %if_join175, %if_join167.outer
  %var_x.0 = phi i256 [ %addition_result, %if_join175 ], [ %var_x.0.ph, %if_join167.outer ]
  %and_result161 = and i256 %var_x.0, 255
  %addition_result = add nsw i256 %and_result161, -1
  %comparison_result171 = icmp eq i256 %addition_result, 0
  br i1 %comparison_result171, label %for_join159, label %if_join175

if_join175:                                       ; preds = %if_join167
  %and_result177 = and i256 %addition_result, 1
  %comparison_result180 = icmp eq i256 %and_result177, 0
  br i1 %comparison_result180, label %if_join167, label %if_join184

if_join184:                                       ; preds = %if_join175
  br i1 %comparison_result187, label %for_join159, label %for_condition192.outer

for_condition192.outer:                           ; preds = %for_condition271, %if_join184
  %var_y.0.ph = phi i256 [ 10, %if_join184 ], [ %addition_result.i427, %for_condition271 ]
  %var_cnt.9.ph = phi i256 [ %var_cnt.7.ph645, %if_join184 ], [ %var_cnt.11, %for_condition271 ]
  br label %for_condition192

for_condition192:                                 ; preds = %for_condition192.backedge, %for_condition192.outer
  %var_y.0 = phi i256 [ %var_y.0.ph, %for_condition192.outer ], [ %addition_result.i427, %for_condition192.backedge ]
  %comparison_result198 = icmp ult i256 %var_y.0, 17
  br i1 %comparison_result198, label %for_body193, label %for_join195

for_body193:                                      ; preds = %for_condition192
  switch i8 %trunc, label %checked_add_t_uint8.exit [
    i8 7, label %return
    i8 27, label %if_join167.outer.backedge
  ]

for_join195:                                      ; preds = %if_join228, %for_condition192
  br i1 %comparison_result330, label %return, label %if_join167.outer.backedge

if_join167.outer.backedge:                        ; preds = %if_join252, %for_join195, %for_body193
  %var_cnt.7.ph645.be = phi i256 [ %var_cnt.9.ph, %for_join195 ], [ %var_cnt.7.ph645, %for_body193 ], [ %var_cnt.9.ph, %if_join252 ]
  br label %if_join167.outer

checked_add_t_uint8.exit:                         ; preds = %for_body193
  %addition_result.i427 = add nuw nsw i256 %var_y.0, 1
  %remainder_result_non_zero.lhs.trunc = trunc nuw i256 %addition_result.i427 to i8
  %remainder_result_non_zero429 = urem i8 %remainder_result_non_zero.lhs.trunc, 3
  %comparison_result224 = icmp eq i8 %remainder_result_non_zero429, 0
  br i1 %comparison_result224, label %for_condition192.backedge, label %if_join228

if_join228:                                       ; preds = %checked_add_t_uint8.exit
  %comparison_result230 = icmp eq i256 %addition_result.i427, 16
  br i1 %comparison_result230, label %for_join195, label %if_join234

if_join234:                                       ; preds = %if_join228
  %comparison_result247 = icmp ugt i256 %var_y.0, 10
  %spec.select402 = and i1 %comparison_result237, %comparison_result247
  br i1 %spec.select402, label %for_condition192.backedge, label %if_join252

for_condition192.backedge:                        ; preds = %if_join234, %checked_add_t_uint8.exit
  br label %for_condition192

if_join252:                                       ; preds = %if_join234
  %comparison_result265 = icmp ugt i256 %var_y.0, 12
  %spec.select403 = and i1 %comparison_result255, %comparison_result265
  br i1 %spec.select403, label %if_join167.outer.backedge, label %for_condition271

for_condition271:                                 ; preds = %for_increment273, %if_join252
  %var_l.0 = phi i256 [ %addition_result326, %for_increment273 ], [ 0, %if_join252 ]
  %var_cnt.11 = phi i256 [ %var_cnt.12, %for_increment273 ], [ %var_cnt.9.ph, %if_join252 ]
  %comparison_result277 = icmp ugt i256 %var_l.0, 3
  %or.cond404 = or i1 %comparison_result282, %comparison_result277
  br i1 %or.cond404, label %for_condition192.outer, label %if_join286

for_increment273:                                 ; preds = %if_join318, %if_join295, %if_join286
  %var_cnt.12 = phi i256 [ %var_cnt.11, %if_join286 ], [ %addition_result312, %if_join318 ], [ %var_cnt.11, %if_join295 ]
  %addition_result326 = add nuw nsw i256 %var_l.0, 1
  br label %for_condition271

if_join286:                                       ; preds = %for_condition271
  %and_result288 = and i256 %var_l.0, 1
  %comparison_result291 = icmp eq i256 %and_result288, 0
  br i1 %comparison_result291, label %for_increment273, label %if_join295

if_join295:                                       ; preds = %if_join286
  switch i8 %trunc, label %if_join309 [
    i8 8, label %return
    i8 13, label %for_increment273
  ]

if_join309:                                       ; preds = %if_join295
  %and_result311 = and i256 %var_cnt.11, 18446744073709551615
  %comparison_result314 = icmp eq i256 %and_result311, 18446744073709551615
  br i1 %comparison_result314, label %shift_left_non_overflow.i411, label %if_join318

if_join318:                                       ; preds = %if_join309
  %addition_result312 = add nuw nsw i256 %and_result311, 1
  br label %for_increment273
}
