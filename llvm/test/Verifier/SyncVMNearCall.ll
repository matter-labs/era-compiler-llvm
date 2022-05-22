; RUN: not llvm-as < %s 2>&1 | FileCheck %s

target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

declare void @foo()
declare void @bar(i1, i256, i256*, {i256, i256}*)

define i256 @callee_is_absent() {
; CHECK: llvm.syncvm.nearcall parameter #1 must be a statically known function pointer bitcasted to i256*
  %1 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* null, i256 42)
	ret i256 %1
}

define i256 @callee_does_not_known_statically(i256()* %foo) {
  %1 = bitcast i256()* %foo to i256*
; CHECK: llvm.syncvm.nearcall parameter #1 must be a statically known function pointer bitcasted to i256*
  %2 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %1, i256 42)
  ret i256 %2
}

define i256 @number_of_parameters_mismatch() {
  %1 = bitcast void()* @foo to i256*
; CHECK: llvm.syncvm.nearcall parameters number should be equal to the number of the callee parameters plus 2 (the callee and abi data)
  %2 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %1, i256 42, i256 0)
  ret i256 %2
}

define i256 @parameters_type_mismatch() {
  %1 = bitcast void(i1, i256, i256*, {i256, i256}*)* @bar to i256*
; CHECK: llvm.syncvm.nearcall paramater #3 doesn't type match the callee parameter #1
  %2 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %1, i256 42, i256 0, i256 0, i256* null, {i256, i256}* null)
  ret i256 %2
}

define i256 @parameters_type_mismatch2() {
  %1 = bitcast void(i1, i256, i256*, {i256, i256}*)* @bar to i256*
; CHECK: llvm.syncvm.nearcall paramater #6 doesn't type match the callee parameter #4
  %2 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %1, i256 42, i1 0, i256 0, i256* null, i256* null)
  ret i256 %2
}

; CHECK: llvm.syncvm.nearcall
; CHECK-NOT: llvm.syncvm.nearcall

define i256 @ok() {
  %1 = bitcast void(i1, i256, i256*, {i256, i256}*)* @bar to i256*
  %2 = call i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* %1, i256 42, i1 0, i256 0, i256* null, {i256, i256}* null)
  ret i256 %2
}

define i256 @ok2() personality i256()* @__personality {
  %1 = invoke i256(i256*, i256, ...) @llvm.syncvm.nearcall(i256* bitcast (void()* @foo to i256*), i256 42)
           to label %success unwind label %failure
success:
  ret i256 %1
failure:
  landingpad {i256, i256} cleanup
  ret i256 0
}

declare i256 @llvm.syncvm.nearcall(i256*, i256, ...)
declare i256 @__personality()
