; RUN: opt -passes=eravm-sha3-constant-folding -S < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

; Both the store instructions and the __sha3 call has constexpr addresses.
define i256 @sha3_test_1() nounwind {
; CHECK-LABEL: @sha3_test_1
; CHECK: ret i256 -53675409633959416604748946233496653964072736789863655143901645101595015023086

entry:
  store i256 304594385234, ptr addrspace(1) null, align 4294967296
  store i256 56457598675863654, ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1)), align 32
  %hash = tail call i256 @__sha3(ptr addrspace(1) null, i256 64, i1 true)
  ret i256 %hash
}

; Both the store instructions and the __sha3 call has runtime addresses.
define i256 @sha3_test_2(ptr addrspace(1) nocapture %addr) nounwind {
; CHECK-LABEL: @sha3_test_2
; CHECK: ret i256 -53675409633959416604748946233496653964072736789863655143901645101595015023086

entry:
  store i256 304594385234, ptr addrspace(1) %addr, align 1
  %next_addr = getelementptr i256, ptr addrspace(1) %addr, i256 1
  store i256 56457598675863654, ptr addrspace(1) %next_addr, align 1
  %hash = tail call fastcc i256 @__sha3(ptr addrspace(1) %addr, i256 64, i1 true)
  ret i256 %hash
}

; Store instructions don't cover __sha3 memory location, so no constant folding.
define i256 @sha3_test_3(ptr addrspace(1) nocapture %addr) nounwind {
; CHECK-LABEL: @sha3_test_3
; CHECK: {{.*call.*}} i256 @__sha3

entry:
  store i256 0, ptr addrspace(1) %addr, align 1
  %next_addr = getelementptr i256, ptr addrspace(1) %addr, i256 1
  store i256 304594385234, ptr addrspace(1) %next_addr, align 1
  %next_addr2 = getelementptr i256, ptr addrspace(1) %addr, i256 3
  store i256 56457598675863654, ptr addrspace(1) %next_addr2, align 1
  %hash = tail call fastcc i256 @__sha3(ptr addrspace(1) %addr, i256 96, i1 true)
  ret i256 %hash
}

; The second store partially overlaps __sha3 memory location,
; so no constant folding.
define i256 @sha3_test_4(ptr addrspace(1) nocapture %addr) nounwind {
; CHECK-LABEL: @sha3_test_4
; CHECK: {{.*call.*}} i256 @__sha3

entry:
  store i256 0, ptr addrspace(1) %addr, align 1
  %next_addr = getelementptr i256, ptr addrspace(1) %addr, i256 1
  store i512 304594385234, ptr addrspace(1) %next_addr, align 1
  %hash = tail call fastcc i256 @__sha3(ptr addrspace(1) %addr, i256 64, i1 true)
  ret i256 %hash
}

; Store instructions have different store sizes.
define i256 @sha3_test_5(ptr addrspace(1) nocapture %addr) nounwind {
; CHECK-LABEL: @sha3_test_5
; CHECK: ret i256 -53675409633959416604748946233496653964072736789863655143901645101595015023086

entry:
  store i128 0, ptr addrspace(1) %addr, align 1
  %next_addr = getelementptr i8, ptr addrspace(1) %addr, i256 16
  store i128 304594385234, ptr addrspace(1) %next_addr, align 1
  %next_addr2 = getelementptr i8, ptr addrspace(1) %addr, i256 32
  store i256 56457598675863654, ptr addrspace(1) %next_addr2, align 1
  %hash = tail call fastcc i256 @__sha3(ptr addrspace(1) %addr, i256 64, i1 true)
  ret i256 %hash
}

; Only the first store is used for the constant folding.
define i256 @sha3_test_6(ptr addrspace(1) nocapture %addr) nounwind {
; CHECK-LABEL: @sha3_test_6
; CHECK: ret i256 -1651279235167815098054286291856006982035426946965232889084721396369881222887

entry:
  store i256 304594385234, ptr addrspace(1) %addr, align 1
  %next_addr = getelementptr i256, ptr addrspace(1) %addr, i256 1
  store i256 56457598675863654, ptr addrspace(1) %next_addr, align 1
  %hash = tail call fastcc i256 @__sha3(ptr addrspace(1) %addr, i256 32, i1 true)
  ret i256 %hash
}

; The second sha3 call gets folded, but not the first one because there is
; non-analyzable clobber.
define i256 @sha3_test_7(ptr addrspace(1) nocapture %addr, ptr addrspace(1) nocapture %addr2) nounwind {
; CHECK-LABEL: @sha3_test_7
; CHECK: %hash1 = {{.*call.*}} @__sha3
; CHECK: add i256 %hash1, 28454950007360609575222453380260700122861180288886985272557645317297017637223

entry:
  store i256 304594385234, ptr addrspace(1) %addr, align 1
  %next_addr = getelementptr i256, ptr addrspace(1) %addr, i256 1
  store i256 111, ptr addrspace(1) %addr2, align 1
  store i256 56457598675863654, ptr addrspace(1) %next_addr, align 1
  %hash1 = tail call fastcc i256 @__sha3(ptr addrspace(1) %addr, i256 64, i1 true)
  %hash2 = tail call fastcc i256 @__sha3(ptr addrspace(1) %next_addr, i256 32, i1 true)
  %hash = add i256 %hash1, %hash2
  ret i256 %hash
}

; Memory locations of store instructions do alias with each other, so no
; constant folding. Theoretically we can support this case. TODO: CPR-1370.
define i256 @sha3_test_8(ptr addrspace(1) nocapture %addr) nounwind {
; CHECK-LABEL: @sha3_test_8
; CHECK: {{.*call.*}} i256 @__sha3

entry:
  store i256 0, ptr addrspace(1) %addr, align 1
  %next_addr = getelementptr i8, ptr addrspace(1) %addr, i256 31
  store i256 304594385234, ptr addrspace(1) %next_addr, align 1
  %next_addr2 = getelementptr i8, ptr addrspace(1) %addr, i256 63
  store i8 17, ptr addrspace(1) %next_addr2, align 1
  %hash = tail call fastcc i256 @__sha3(ptr addrspace(1) %addr, i256 64, i1 true)
  ret i256 %hash
}

; We have two __sha3 calls where the second call gets folded on the second iteration.
define i256 @sha3_test_9(ptr addrspace(1) %addr) nounwind {
; CHECK-LABEL: @sha3_test_9
; CHECK-NOT: {{.*call.*}} i256 @__sha3

entry:
  store i256 304594385234, ptr addrspace(1) %addr, align 1
  %next_addr = getelementptr i256, ptr addrspace(1) %addr, i256 1
  store i256 56457598675863654, ptr addrspace(1) %next_addr, align 1
  %hash = tail call fastcc i256 @__sha3(ptr addrspace(1) %addr, i256 64, i1 true)
  %sum = add i256 %hash, 10
  store i256 %sum, ptr addrspace(1) %next_addr, align 1
  store i256 111111111111, ptr addrspace(1) %addr, align 1
  %hash2 = tail call fastcc i256 @__sha3(ptr addrspace(1) %addr, i256 64, i1 true)
  ret i256 %hash2
}

; Offset of the second store is too big (requires > 64 bits), so no constant folding.
define i256 @sha3_test_10() nounwind {
; CHECK-LABEL: @sha3_test_10
; CHECK: {{.*call.*}} i256 @__sha3

entry:
  store i256 304594385234, ptr addrspace(1) null, align 4294967296
  store i256 56457598675863654, ptr addrspace(1) inttoptr (i256 18446744073709551616 to ptr addrspace(1)), align 32
  %hash = tail call i256 @__sha3(ptr addrspace(1) null, i256 64, i1 true)
  ret i256 %hash
}

declare i256 @__sha3(ptr addrspace(1), i256, i1) #0

attributes #0 = { argmemonly nofree null_pointer_is_valid readonly }
