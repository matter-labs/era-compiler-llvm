; RUN: opt -passes=eravm-lower-intrinsics -S < %s | FileCheck %s
; RUN: llc -O3 < %s | FileCheck --check-prefix=CHECK-INSTRS %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

declare void @llvm.memmove.p1.p1.i256(ptr addrspace(1), ptr addrspace(1), i256, i1 immarg)

define i256 @test_unknown(ptr addrspace(1) %dst, ptr addrspace(1) %src, i256 %size) {
; CHECK-LABEL: @test_unknown(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[LOOP_BYTES_COUNT:%.*]] = and i256 [[SIZE:%.*]], -32
; CHECK-NEXT:    [[RESIDUAL_BYTES:%.*]] = and i256 [[SIZE]], 31
; CHECK-NEXT:    [[COMPARE_SRC_DST:%.*]] = icmp ult ptr addrspace(1) [[SRC:%.*]], [[DST:%.*]]
; CHECK-NEXT:    [[COMPARE_LCB_TO_0:%.*]] = icmp eq i256 [[LOOP_BYTES_COUNT]], 0
; CHECK-NEXT:    [[COMPARE_RB_TO_0:%.*]] = icmp eq i256 [[RESIDUAL_BYTES]], 0
; CHECK-NEXT:    br i1 [[COMPARE_SRC_DST]], label [[COPY_BACKWARDS:%.*]], label [[COPY_FORWARD:%.*]]
; CHECK:       copy-backwards:
; CHECK-NEXT:    br i1 [[COMPARE_LCB_TO_0]], label [[COPY_BACKWARDS_RESIDUAL_COND:%.*]], label [[COPY_BACKWARDS_LOOP_PREHEADER:%.*]]
; CHECK:       copy-backwards-residual-cond:
; CHECK-NEXT:    br i1 [[COMPARE_RB_TO_0]], label [[MEMMOVE_DONE:%.*]], label [[MEMMOVE_RESIDUAL:%.*]]
; CHECK:       copy-backwards-loop-preheader:
; CHECK-NEXT:    [[SRC_BWD_RES_END:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[SRC]], i256 [[RESIDUAL_BYTES]]
; CHECK-NEXT:    [[SRC_BWD_START:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[SRC_BWD_RES_END]], i256 -32
; CHECK-NEXT:    [[DST_BWD_RES_END:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[DST]], i256 [[RESIDUAL_BYTES]]
; CHECK-NEXT:    [[DST_BWD_START:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[DST_BWD_RES_END]], i256 -32
; CHECK-NEXT:    br label [[COPY_BACKWARDS_LOOP:%.*]]
; CHECK:       copy-backwards-loop:
; CHECK-NEXT:    [[BYTES_COUNT:%.*]] = phi i256 [ [[DECREMENT_BYTES:%.*]], [[COPY_BACKWARDS_LOOP]] ], [ [[LOOP_BYTES_COUNT]], [[COPY_BACKWARDS_LOOP_PREHEADER]] ]
; CHECK-NEXT:    [[LOAD_ADDR:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[SRC_BWD_START]], i256 [[BYTES_COUNT]]
; CHECK-NEXT:    [[ELEMENT:%.*]] = load i256, ptr addrspace(1) [[LOAD_ADDR]], align 1
; CHECK-NEXT:    [[STORE_ADDR:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[DST_BWD_START]], i256 [[BYTES_COUNT]]
; CHECK-NEXT:    store i256 [[ELEMENT]], ptr addrspace(1) [[STORE_ADDR]], align 1
; CHECK-NEXT:    [[DECREMENT_BYTES]] = sub i256 [[BYTES_COUNT]], 32
; CHECK-NEXT:    [[COMPARE_BYTES:%.*]] = icmp eq i256 [[DECREMENT_BYTES]], 0
; CHECK-NEXT:    br i1 [[COMPARE_BYTES]], label [[COPY_BACKWARDS_RESIDUAL_COND]], label [[COPY_BACKWARDS_LOOP]]
; CHECK:       copy-forward:
; CHECK-NEXT:    br i1 [[COMPARE_LCB_TO_0]], label [[COPY_FORWARD_RESIDUAL_COND:%.*]], label [[COPY_FORWARD_LOOP_PREHEADER:%.*]]
; CHECK:       copy-forward-residual-cond:
; CHECK-NEXT:    br i1 [[COMPARE_RB_TO_0]], label [[MEMMOVE_DONE]], label [[COPY_FORWARD_RESIDUAL:%.*]]
; CHECK:       copy-forward-loop-preheader:
; CHECK-NEXT:    br label [[COPY_FORWARD_LOOP:%.*]]
; CHECK:       copy-forward-loop:
; CHECK-NEXT:    [[BYTES_COUNT1:%.*]] = phi i256 [ [[INCREMENT_BYTES:%.*]], [[COPY_FORWARD_LOOP]] ], [ 0, [[COPY_FORWARD_LOOP_PREHEADER]] ]
; CHECK-NEXT:    [[LOAD_ADDR2:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[SRC]], i256 [[BYTES_COUNT1]]
; CHECK-NEXT:    [[ELEMENT3:%.*]] = load i256, ptr addrspace(1) [[LOAD_ADDR2]], align 1
; CHECK-NEXT:    [[STORE_ADDR4:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[DST]], i256 [[BYTES_COUNT1]]
; CHECK-NEXT:    store i256 [[ELEMENT3]], ptr addrspace(1) [[STORE_ADDR4]], align 1
; CHECK-NEXT:    [[INCREMENT_BYTES]] = add i256 [[BYTES_COUNT1]], 32
; CHECK-NEXT:    [[COMPARE_BYTES5:%.*]] = icmp eq i256 [[INCREMENT_BYTES]], [[LOOP_BYTES_COUNT]]
; CHECK-NEXT:    br i1 [[COMPARE_BYTES5]], label [[COPY_FORWARD_LOOP_EXIT:%.*]], label [[COPY_FORWARD_LOOP]]
; CHECK:       copy-forward-loop-exit:
; CHECK-NEXT:    br label [[COPY_FORWARD_RESIDUAL_COND]]
; CHECK:       copy-forward-residual:
; CHECK-NEXT:    [[SRC_FWD_RES_ADDR:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[SRC]], i256 [[LOOP_BYTES_COUNT]]
; CHECK-NEXT:    [[DST_FWD_RES_ADR:%.*]] = getelementptr inbounds i8, ptr addrspace(1) [[DST]], i256 [[LOOP_BYTES_COUNT]]
; CHECK-NEXT:    br label [[MEMMOVE_RESIDUAL]]
; CHECK:       memmove-residual:
; CHECK-NEXT:    [[SRC_RES_ADDR:%.*]] = phi ptr addrspace(1) [ [[SRC_FWD_RES_ADDR]], [[COPY_FORWARD_RESIDUAL]] ], [ [[SRC]], [[COPY_BACKWARDS_RESIDUAL_COND]] ]
; CHECK-NEXT:    [[DST_RES_ADDR:%.*]] = phi ptr addrspace(1) [ [[DST_FWD_RES_ADR]], [[COPY_FORWARD_RESIDUAL]] ], [ [[DST]], [[COPY_BACKWARDS_RESIDUAL_COND]] ]
; CHECK-NEXT:    [[SRC_LOAD:%.*]] = load i256, ptr addrspace(1) [[SRC_RES_ADDR]], align 1
; CHECK-NEXT:    [[RES_BITS:%.*]] = mul i256 8, [[RESIDUAL_BYTES]]
; CHECK-NEXT:    [[UPPER_BITS:%.*]] = sub i256 256, [[RES_BITS]]
; CHECK-NEXT:    [[SRC_MASK:%.*]] = shl i256 -1, [[UPPER_BITS]]
; CHECK-NEXT:    [[SRC_MASKED:%.*]] = and i256 [[SRC_LOAD]], [[SRC_MASK]]
; CHECK-NEXT:    [[DST_LOAD:%.*]] = load i256, ptr addrspace(1) [[DST_RES_ADDR]], align 1
; CHECK-NEXT:    [[DST_MASK:%.*]] = lshr i256 -1, [[RES_BITS]]
; CHECK-NEXT:    [[DST_MASKED:%.*]] = and i256 [[DST_LOAD]], [[DST_MASK]]
; CHECK-NEXT:    [[STORE_ELEMENT:%.*]] = or i256 [[SRC_MASKED]], [[DST_MASKED]]
; CHECK-NEXT:    store i256 [[STORE_ELEMENT]], ptr addrspace(1) [[DST_RES_ADDR]], align 1
; CHECK-NEXT:    br label [[MEMMOVE_DONE]]
; CHECK:       memmove-done:
; CHECK-NEXT:    ret i256 0

; CHECK-INSTRS-LABEL: test_unknown:

; Preheader and backwards loop.
; CHECK-INSTRS:       add r2, r3, r6
; CHECK-INSTRS-NEXT:  add r1, r3, r5
; CHECK-INSTRS-NEXT:  sub.s 32, r5, r5
; CHECK-INSTRS-NEXT:  sub.s 32, r6, r6
; CHECK-INSTRS-NEXT:  .BB0_4:
; CHECK-INSTRS:       add r5, r4, r7
; CHECK-INSTRS-NEXT:  add r6, r4, r8
; CHECK-INSTRS-NEXT:  ld.1 r8, r8
; CHECK-INSTRS-NEXT:  st.1 r7, r8
; CHECK-INSTRS-NEXT:  sub.s! 32, r4, r4
; CHECK-INSTRS-NEXT:  jump.ne @.BB0_4

; Preheader and forward loop.
; CHECK-INSTRS:       add r0, r0, r5
; CHECK-INSTRS-NEXT:  add r2, r0, r6
; CHECK-INSTRS-NEXT:  add r1, r0, r7
; CHECK-INSTRS-NEXT:  .BB0_8:
; CHECK-INSTRS:       ld.1.inc r6, r8, r6
; CHECK-INSTRS-NEXT:  st.1.inc r7, r8, r7
; CHECK-INSTRS-NEXT:  add 32, r5, r5
; CHECK-INSTRS-NEXT:  sub! r5, r4, r8
; CHECK-INSTRS-NEXT:  jump.ne @.BB0_8

; Copy forward residual and residual.
; CHECK-INSTRS:       add r1, r4, r1
; CHECK-INSTRS-NEXT:  add r2, r4, r2
; CHECK-INSTRS-NEXT:  .BB0_10:
; CHECK-INSTRS:       shl.s 3, r3, r3
; CHECK-INSTRS-NEXT:  ld.1 r1, r4
; CHECK-INSTRS-NEXT:  shl r4, r3, r4
; CHECK-INSTRS-NEXT:  shr r4, r3, r4
; CHECK-INSTRS-NEXT:  ld.1 r2, r2
; CHECK-INSTRS-NEXT:  sub 256, r3, r3
; CHECK-INSTRS-NEXT:  shr r2, r3, r2
; CHECK-INSTRS-NEXT:  shl r2, r3, r2
; CHECK-INSTRS-NEXT:  or r2, r4, r2
; CHECK-INSTRS-NEXT:  st.1 r1, r2
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) %dst, ptr addrspace(1) %src, i256 %size, i1 false)
  ret i256 0
}

; Test that we called memcpy implementation.
define i256 @test_known_forward() {
; CHECK-LABEL: @test_known_forward(
; CHECK:       load-store-loop:

; CHECK-INSTRS-LABEL: test_known_forward:
; CHECK-INSTRS:       add 10, r0, r1
; CHECK-INSTRS-NEXT:  add 100, r0, r2
; CHECK-INSTRS-NEXT:  add r0, r0, r3
; CHECK-INSTRS-NEXT:  .BB1_1:
; CHECK-INSTRS:       ld.1.inc r2, r4, r2
; CHECK-INSTRS-NEXT:  st.1.inc r1, r4, r1
; CHECK-INSTRS-NEXT:  add 1, r3, r3
; CHECK-INSTRS-NEXT:  sub.s! 2, r3, r4
; CHECK-INSTRS-NEXT:  jump.lt @.BB1_1
; CHECK-INSTRS:       sub.s 256, r0, r1
; CHECK-INSTRS-NEXT:  ld.1 164, r2
; CHECK-INSTRS-NEXT:  and r2, r1, r1
; CHECK-INSTRS-NEXT:  ld.1 74, r2
; CHECK-INSTRS-NEXT:  and 255, r2, r2
; CHECK-INSTRS-NEXT:  or r1, r2, r1
; CHECK-INSTRS-NEXT:  st.1 74, r1
; CHECK-INSTRS-NEXT:  add r0, r0, r1
; CHECK-INSTRS-NEXT:  ret
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), i256 95, i1 false)
  ret i256 0
}

define i256 @test_known_backward() {
; CHECK-LABEL: @test_known_backward(
; CHECK:       copy-backwards:

; CHECK-INSTRS-LABEL: test_known_backward:
; CHECK-INSTRS:       add 64, r0, r1
; CHECK-INSTRS-NEXT:  .BB2_2:
; CHECK-INSTRS:       add 9, r1, r2
; CHECK-INSTRS-NEXT:  ld.1 r2, r2
; CHECK-INSTRS-NEXT:  add 99, r1, r3
; CHECK-INSTRS-NEXT:  st.1 r3, r2
; CHECK-INSTRS-NEXT:  sub.s! 32, r1, r1
; CHECK-INSTRS-NEXT:  jump.ne @.BB2_2
; CHECK-INSTRS:       sub.s 256, r0, r1
; CHECK-INSTRS-NEXT:  ld.1 10, r2
; CHECK-INSTRS-NEXT:  and r2, r1, r1
; CHECK-INSTRS-NEXT:  ld.1 100, r2
; CHECK-INSTRS-NEXT:  and 255, r2, r2
; CHECK-INSTRS-NEXT:  or r1, r2, r1
; CHECK-INSTRS-NEXT:  st.1 100, r1
; CHECK-INSTRS-NEXT:  add r0, r0, r1
; CHECK-INSTRS-NEXT:  ret
;
entry:
  tail call void @llvm.memmove.p1.p1.i256(ptr addrspace(1) inttoptr (i256 100 to ptr addrspace(1)), ptr addrspace(1) inttoptr (i256 10 to ptr addrspace(1)), i256 95, i1 false)
  ret i256 0
}
