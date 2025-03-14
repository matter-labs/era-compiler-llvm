target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"
declare i256 @llvm.evm.loadimmutable(metadata)

define i256 @runtime() {
  %res = call i256 @llvm.evm.loadimmutable(metadata !1)
  ret i256 %res
}
!1 = !{!"40"}
