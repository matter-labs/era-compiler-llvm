; RUN: llc -O0  < %s | FileCheck %s

target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm-unknown-unknown"

declare i32 @__personality()

; CHECK-LABEL: test256bit
; Function Attrs: null_pointer_is_valid
define ptr addrspace(3) @test256bit() #0 personality ptr @__personality {
entry:
  ; CHECK: addp.s
  %val = getelementptr i8, ptr addrspace(3) undef, i256 3334353453452342342354355544445321191012012
  ret ptr addrspace(3) %val
}

; CHECK-LABEL: test128bit
; Function Attrs: null_pointer_is_valid
define ptr addrspace(3) @test128bit() #0 personality ptr @__personality {
entry:
  ; CHECK: addp.s
  %val = getelementptr i8, ptr addrspace(3) undef, i128 170141183460469231731687303715884105727
  ret ptr addrspace(3) %val
}

attributes #0 = { null_pointer_is_valid }
