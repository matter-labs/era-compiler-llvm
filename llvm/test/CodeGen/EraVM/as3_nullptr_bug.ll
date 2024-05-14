; RUN: llc < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

; Function Attrs: null_pointer_is_valid
define i256 @__entry() local_unnamed_addr #0 {
entry:
  ; address space 3 pointers are not numeric, but as an exception it's ok to materialize nullptr from 0.
  ; CHECK: ldp r0, r1
  %calldata_value = load i256, ptr addrspace(3) null, align 32
  %comparison_result = icmp ugt i256 %calldata_value, 340282366920938463463374607431768211455
  %return_value = select i1 %comparison_result, i256 16, i256 0
  ret i256 %return_value
}

attributes #0 = { null_pointer_is_valid }
