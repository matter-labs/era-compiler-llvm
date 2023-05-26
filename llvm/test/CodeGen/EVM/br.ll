; RUN: llc --mtriple=evm < %s | FileCheck %s

define i256 @diamond(i256 %rs1, i256 %rs2) nounwind {
; CHECK-LABEL: diamond

  %cmp = icmp eq i256 %rs1, %rs2
; CHECK: EQ
; CHECK: ISZERO
; CHECK: JUMPI
  br i1 %cmp, label %true_bb, label %false_bb

true_bb:
  %mul = mul i256 %rs1, %rs1
  br label %end_bb

false_bb:
  %add = add i256 %rs1, %rs2
  br label %end_bb

end_bb:
  %res = phi i256 [%mul, %true_bb], [%add, %false_bb]
  ret i256 %res
}


define i256 @loop(i256 %p1) nounwind {
; CHECK-LABEL: loop
entry:
  br label %loop.cond

loop.cond:
  %i = phi i256 [0, %entry], [%i.next, %loop.body]
  %res = phi i256 [0, %entry], [%res.next, %loop.body]
; CHECK: EQ
; CHECK: JUMPI
  %cond = icmp ne i256 %i, %p1
  br i1 %cond, label %loop.body, label %loop.exit

loop.body:
  %i.next = add i256 %i, 1
  %res.next = add i256 %res, %i
; CHECK: JUMP
  br label %loop.cond

loop.exit:
  ret i256 %res
}
