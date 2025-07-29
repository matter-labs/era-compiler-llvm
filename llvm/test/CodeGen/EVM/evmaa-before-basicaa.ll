; REQUIRES: asserts
; RUN: opt -aa-pipeline=default -passes='require<aa>' -debug-pass-manager -disable-output -S < %s 2>&1 | FileCheck %s
; RUN: llc --debug-only='aa' -o /dev/null %s 2>&1 | FileCheck %s -check-prefix=LEGACY

; In default AA pipeline, EVMAA should run before BasicAA to reduce compile time for EVM backend
target triple = "evm"

; CHECK: Running analysis: EVMAA on foo
; CHECK-NEXT: Running analysis: BasicAA on foo

; LEGACY: AAResults register Early ExternalAA: EVM Address space based Alias Analysis Wrapper
; LEGACY-NEXT: AAResults register BasicAA
define void @foo(){
entry:
  ret void
}

