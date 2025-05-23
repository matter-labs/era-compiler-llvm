; RUN: llc -O3 < %s

; This test case is reduced with llvm-reduce.
; Before the fix, we had an infinite loop in EVMStackSolver::runPropagation,
; because EarlyTailDuplicate merged header and latch blocks. This caused
; MachineLoopInfo not to detect the loop correctly, thus running into an
; infinite loop.

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

; Function Attrs: nounwind willreturn memory(none)
declare i256 @llvm.evm.signextend(i256, i256) #0

define void @test(i1 %comparison_result6, i256 %mulmod1446) {
entry:
  br i1 %comparison_result6, label %"block_rt_2/0", label %shift_right_non_overflow

"block_rt_2/0":                                   ; preds = %entry
  unreachable

"block_rt_66/0":                                  ; preds = %division_join1531, %shift_right_non_overflow
  br i1 %division_is_divider_zero1455, label %division_join1453, label %division_join1531

shift_right_non_overflow:                         ; preds = %entry
  %division_is_divider_zero1455 = icmp eq i256 %mulmod1446, 0
  br label %"block_rt_66/0"

division_join1453:                                ; preds = %"block_rt_66/0"
  %signextend1463 = tail call i256 @llvm.evm.signextend(i256 0, i256 0)
  br label %division_join1531

division_join1531:                                ; preds = %division_join1453, %"block_rt_66/0"
  store i256 0, ptr addrspace(1) null, align 4294967296
  br label %"block_rt_66/0"
}

attributes #0 = { nounwind willreturn memory(none) }
