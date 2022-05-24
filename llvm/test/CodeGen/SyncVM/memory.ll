; RUN: llc < %s | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

; ============================ i256 aligned load ==============================
; CHECK-LABEL: load_as0
define i256 @load_as0(i256* %addr) nounwind {
; CHECK: div.s 32, r1, r1, r0
; CHECK: add stack[r1 - 0], r0, r1
  %1 = load i256, i256* %addr, align 32
  ret i256 %1
}

; CHECK-LABEL: load_as1
define i256 @load_as1(i256 addrspace(1)* %addr) nounwind {
; CHECK: uma.heap_read r1, r0, r1
  %1 = load i256, i256 addrspace(1)* %addr, align 32
  ret i256 %1
}

; CHECK-LABEL: load_as2
define i256 @load_as2(i256 addrspace(2)* %addr) nounwind {
; CHECK: uma.calldata_read r1, r0, r1
  %1 = load i256, i256 addrspace(2)* %addr, align 32
  ret i256 %1
}

; CHECK-LABEL: load_as3
define i256 @load_as3(i256 addrspace(3)* %addr) nounwind {
; CHECK: uma.returndata_read r1, r0, r1
  %1 = load i256, i256 addrspace(3)* %addr, align 32
  ret i256 %1
}

; =========================== i256 unaligned load =============================
; CHECK-LABEL: uload_as0
define i256 @uload_as0(i256* %addr) nounwind {
; CHECK: near_call r0, @__unaligned_load_as0, @DEFAULT_UNWIND
  %1 = load i256, i256* %addr, align 1
  ret i256 %1
}

; CHECK-LABEL: uload_as1
define i256 @uload_as1(i256 addrspace(1)* %addr) nounwind {
; CHECK: uma.heap_read r1, r0, r1
  %1 = load i256, i256 addrspace(1)* %addr, align 1
  ret i256 %1
}

; CHECK-LABEL: uload_as2
define i256 @uload_as2(i256 addrspace(2)* %addr) nounwind {
; CHECK: uma.calldata_read r1, r0, r1
  %1 = load i256, i256 addrspace(2)* %addr, align 1
  ret i256 %1
}

; CHECK-LABEL: uload_as3
define i256 @uload_as3(i256 addrspace(3)* %addr) nounwind {
; CHECK: uma.returndata_read r1, r0, r1
  %1 = load i256, i256 addrspace(3)* %addr, align 1
  ret i256 %1
}

; ============================ i256 aligned store =============================
; CHECK-LABEL: store_as0
define void @store_as0(i256* %addr, i256 %val) nounwind {
; CHECK: div.s 32, r1, r1, r0
; CHECK: add r2, r0, stack[r1 - 0]
  store i256 %val, i256* %addr, align 32
  ret void
}

; CHECK-LABEL: store_as1
define void @store_as1(i256 addrspace(1)* %addr, i256 %val) nounwind {
; CHECK: uma.heap_write r1, r2, r0
  store i256 %val, i256 addrspace(1)* %addr, align 32
  ret void
}

; =========================== i256 unaligned store ============================
; CHECK-LABEL: ustore_as0
define void @ustore_as0(i256* %addr, i256 %val) nounwind {
; CHECK: near_call r0, @__unaligned_store_as0, @DEFAULT_UNWIND
  store i256 %val, i256* %addr, align 1
  ret void
}

; CHECK-LABEL: ustore_as1
define void @ustore_as1(i256 addrspace(1)* %addr, i256 %val) nounwind {
; CHECK: uma.heap_write r1, r2, r0
  store i256 %val, i256 addrspace(1)* %addr, align 1
  ret void
}

; ================================= i16 load ==================================
; CHECK-LABEL: load16_as0
define i16 @load16_as0(i16* %addr) nounwind {
; CHECK: add 16, r0, r2
; CHECK: near_call r0, @__small_load_as0, @DEFAULT_UNWIND
  %1 = load i16, i16* %addr, align 1
  ret i16 %1
}

; CHECK-LABEL: load16_as1
define i16 @load16_as1(i16 addrspace(1)* %addr) nounwind {
; CHECK: uma.heap_read r1, r0, r1
; CHECK: shr.s 240, r1, r1
  %1 = load i16, i16 addrspace(1)* %addr, align 1
  ret i16 %1
}

; CHECK-LABEL: load16_as2
define i16 @load16_as2(i16 addrspace(2)* %addr) nounwind {
; CHCEK: uma.calldata_read r1, r0, r1
; CHCEK: shr.s 240, r1, r1
  %1 = load i16, i16 addrspace(2)* %addr, align 1
  ret i16 %1
}

; CHECK-LABEL: load16_as3
define i16 @load16_as3(i16 addrspace(3)* %addr) nounwind {
; CHECK: uma.returndata_read r1, r0, r1
; CHECK: shr.s 240, r1, r1
  %1 = load i16, i16 addrspace(3)* %addr, align 1
  ret i16 %1
}

; =============================== i16 zextload ================================
; CHECK-LABEL: zextload16_as0
define i256 @zextload16_as0(i16* %addr) nounwind {
; CHECK: add 16, r0, r2
; CHECK: near_call r0, @__small_load_as0, @DEFAULT_UNWIND
  %1 = load i16, i16* %addr, align 1
  %2 = zext i16 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: zextload16_as1
define i256 @zextload16_as1(i16 addrspace(1)* %addr) nounwind {
; CHECK: uma.heap_read r1, r0, r1
; CHECK: shr.s 240, r1, r1
  %1 = load i16, i16 addrspace(1)* %addr, align 1
  %2 = zext i16 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: zextload16_as2
define i256 @zextload16_as2(i16 addrspace(2)* %addr) nounwind {
; CHCEK: uma.calldata_read r1, r0, r1
; CHCEK: shr.s 240, r1, r1
  %1 = load i16, i16 addrspace(2)* %addr, align 1
  %2 = zext i16 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: zextload16_as3
define i256 @zextload16_as3(i16 addrspace(3)* %addr) nounwind {
; CHECK: uma.returndata_read r1, r0, r1
; CHECK: shr.s 240, r1, r1
  %1 = load i16, i16 addrspace(3)* %addr, align 1
  %2 = zext i16 %1 to i256
  ret i256 %2
}

; =============================== i16 sextload ================================
; CHECK-LABEL: sextload16_as0
define i256 @sextload16_as0(i16* %addr) nounwind {
; CHECK: add 16, r0, r2
; CHECK: near_call r0, @__small_load_as0, @DEFAULT_UNWIND
  %1 = load i16, i16* %addr, align 1
  %2 = sext i16 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: sextload16_as1
define i256 @sextload16_as1(i16 addrspace(1)* %addr) nounwind {
; CHECK: uma.heap_read r1, r0, r1
  %1 = load i16, i16 addrspace(1)* %addr, align 1
  %2 = sext i16 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: sextload16_as2
define i256 @sextload16_as2(i16 addrspace(2)* %addr) nounwind {
; CHCEK: uma.calldata_read r1, r0, r1
  %1 = load i16, i16 addrspace(2)* %addr, align 1
  %2 = sext i16 %1 to i256
  ret i256 %2
}

; CHECK-LABEL: sextload16_as3
define i256 @sextload16_as3(i16 addrspace(3)* %addr) nounwind {
; CHECK: uma.returndata_read r1, r0, r1
  %1 = load i16, i16 addrspace(3)* %addr, align 1
  %2 = sext i16 %1 to i256
  ret i256 %2
}

; ================================= i16 store =================================
; CHECK-LABEL: store16_as0
define void @store16_as0(i16* %addr, i16 %val) nounwind {
; CHECK: and 65535, r2, r2
; CHECK: add 16, r0, r3
; CHECK: near_call r0, @__small_store_as0, @DEFAULT_UNWIND
  store i16 %val, i16* %addr, align 1
  ret void
}

; CHECK-LABEL: store16_as1
define void @store16_as1(i16 addrspace(1)* %addr, i16 %val) nounwind {
; CHECK: and 65535, r2, r2
; CHECK: add 16, r0, r3
; CHECK: near_call r0, @__small_store_as1, @DEFAULT_UNWIND
  store i16 %val, i16 addrspace(1)* %addr, align 1
  ret void
}

; ============================= i16 trunc store ===============================
; CHECK-LABEL: truncstore16_as0
define void @truncstore16_as0(i16* %addr, i256 %val.1) nounwind {
  %val = trunc i256 %val.1 to i16
; CHECK: and 65535, r2, r2
; CHECK: add 16, r0, r3
; CHECK: near_call r0, @__small_store_as0, @DEFAULT_UNWIND
  store i16 %val, i16* %addr, align 1
  ret void
}

; CHECK-LABEL: truncstore16_as1
define void @truncstore16_as1(i16 addrspace(1)* %addr, i256 %val.1) nounwind {
  %val = trunc i256 %val.1 to i16
; CHECK: and 65535, r2, r2
; CHECK: add 16, r0, r3
; CHECK: near_call r0, @__small_store_as1, @DEFAULT_UNWIND
  store i16 %val, i16 addrspace(1)* %addr, align 1
  ret void
}
