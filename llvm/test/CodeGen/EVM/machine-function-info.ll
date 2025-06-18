; RUN: llc -stop-before=finalize-isel < %s | FileCheck --check-prefix=CHECK-PARAMS %s
; RUN: llc -stop-before=finalize-isel < %s | FileCheck --check-prefix=CHECK-PUSHDEPLOY %s
; RUN: llc -stop-after=evm-backward-propagation-stackification < %s | FileCheck --check-prefix=CHECK-STACKIFIED %s

; Check that the machine function info is correctly set up for different functions.

target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

declare i256 @llvm.evm.pushdeployaddress()

define i256 @params(i256 %arg1, i256 %arg2) {
; CHECK-PARAMS-LABEL: name: params
; CHECK-PARAMS: machineFunctionInfo:
; CHECK-PARAMS:   isStackified:    false
; CHECK-PARAMS:   numberOfParameters: 2
; CHECK-PARAMS:   hasPushDeployAddress: false
  ret i256 %arg1
}

define void @pushdeploy() noreturn {
; CHECK-PUSHDEPLOY-LABEL: name: pushdeploy
; CHECK-PUSHDEPLOY: machineFunctionInfo:
; CHECK-PUSHDEPLOY:   isStackified:    false
; CHECK-PUSHDEPLOY:   numberOfParameters: 0
; CHECK-PUSHDEPLOY:   hasPushDeployAddress: true
  %push = call i256 @llvm.evm.pushdeployaddress()
  unreachable
}

define void @stackified() {
; CHECK-STACKIFIED-LABEL: name: stackified
; CHECK-STACKIFIED: machineFunctionInfo:
; CHECK-STACKIFIED:   isStackified:    true
; CHECK-STACKIFIED:   numberOfParameters: 0
; CHECK-STACKIFIED:   hasPushDeployAddress: false
  ret void
}
