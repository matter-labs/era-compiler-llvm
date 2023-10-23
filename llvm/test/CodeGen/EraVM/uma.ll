; RUN: llc -opaque-pointers < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"


; CHECK-LABEL: uma.load_heap
define i256 @uma.load_heap(i256 addrspace(1)* %ptr) nounwind {
; CHECK: ld.1 r1, r1
  %val = load i256, i256 addrspace(1)* %ptr
  ret i256 %val
}

; CHECK-LABEL: umai.load_heap
define i256 @umai.load_heap() nounwind {
; CHECK: ld.1 42, r1
  %val = load i256, i256 addrspace(1)* inttoptr (i256 42 to i256 addrspace(1)*)
  ret i256 %val
}

; CHECK-LABEL: uma.load_heapaux
define i256 @uma.load_heapaux(i256 addrspace(2)* %ptr) nounwind {
; CHECK: ld.2 r1, r1
  %val = load i256, i256 addrspace(2)* %ptr
  ret i256 %val
}

; CHECK-LABEL: umai.load_heapaux
define i256 @umai.load_heapaux(i256 addrspace(2)* %ptr) nounwind {
; CHECK: ld.2 42, r1
  %val = load i256, i256 addrspace(2)* inttoptr (i256 42 to i256 addrspace(2)*)
  ret i256 %val
}

; CHECK-LABEL: uma.load_generic
define i256 @uma.load_generic(i256 addrspace(3)* %ptr) nounwind {
; CHECK: ld r1, r1
  %val = load i256, i256 addrspace(3)* %ptr
  ret i256 %val
}

; CHECK-LABEL: uma.store_heap
define void @uma.store_heap(i256 %val, i256 addrspace(1)* %ptr) nounwind {
; CHECK: st.1 r2, r1
  store i256 %val, i256 addrspace(1)* %ptr, align 1
  ret void
}

; CHECK-LABEL: umai.store_heap
define void @umai.store_heap(i256 %val) nounwind {
; CHECK: st.1 42, r1
  store i256 %val, i256 addrspace(1)* inttoptr (i256 42 to i256 addrspace(1)*), align 1
  ret void
}

; CHECK-LABEL: uma.store_heapaux
define void @uma.store_heapaux(i256 %val, i256 addrspace(2)* %ptr) nounwind {
; CHECK: st.2 r2, r1
  store i256 %val, i256 addrspace(2)* %ptr, align 1
  ret void
}

; CHECK-LABEL: umai.store_heapaux
define void @umai.store_heapaux(i256 %val) nounwind {
; CHECK: st.2 42, r1
  store i256 %val, i256 addrspace(2)* inttoptr (i256 42 to i256 addrspace(2)*), align 1
  ret void
}

; CHECK-LABEL: st1inc
define void @st1inc(i256 addrspace(1)* %addr, i256 %val) nounwind {
 store i256 %val, i256 addrspace(1)* %addr
; CHECK: st.1.inc r1, r2, r1
; CHECK: st.1 r1, r2
 %newaddr1 = getelementptr i256, i256 addrspace(1)* %addr, i256 1
 %na1 = add i256 %val, 1024
 store i256 %na1, i256 addrspace(1)* %newaddr1
 ret void
}

; CHECK-LABEL: st2inc
define void @st2inc(i256 addrspace(2)* %addr, i256 %val) nounwind {
 store i256 %val, i256 addrspace(2)* %addr
; CHECK: st.2.inc r1, r2, r1
; CHECK: st.2 r1, r2
 %newaddr1 = getelementptr i256, i256 addrspace(2)* %addr, i256 1
 %na1 = add i256 %val, 1024
 store i256 %na1, i256 addrspace(2)* %newaddr1
 ret void
}

; CHECK-LABEL: ldinc
define i256 @ldinc(i256 addrspace(3)* %addr) nounwind {
; CHECK: ld.inc r1, r1, r2
; CHECK: ld r2, r2
 %val = load i256, i256 addrspace(3)* %addr
 %newaddr1 = getelementptr i256, i256 addrspace(3)* %addr, i256 1
 %val1 = load i256, i256 addrspace(3)* %newaddr1
 %sum = add i256 %val, %val1
 ret i256 %sum
}

; CHECK-LABEL: ld1inc
define i256 @ld1inc(i256 addrspace(1)* %addr) nounwind {
; CHECK: ld.1.inc r1, r1, r2
; CHECK: ld.1 r2, r2
 %val = load i256, i256 addrspace(1)* %addr
 %newaddr1 = getelementptr i256, i256 addrspace(1)* %addr, i256 1
 %val1 = load i256, i256 addrspace(1)* %newaddr1
 %sum = add i256 %val, %val1
 ret i256 %sum
}

; CHECK-LABEL: ld2inc
define i256 @ld2inc(i256 addrspace(2)* %addr) nounwind {
; CHECK: ld.2.inc r1, r1, r2
; CHECK: ld.2 r2, r2
 %val = load i256, i256 addrspace(2)* %addr
 %newaddr1 = getelementptr i256, i256 addrspace(2)* %addr, i256 1
 %val1 = load i256, i256 addrspace(2)* %newaddr1
 %sum = add i256 %val, %val1
 ret i256 %sum
}

