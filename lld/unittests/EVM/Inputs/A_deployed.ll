target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"
declare i256 @llvm.evm.linkersymbol(metadata)
declare i256 @llvm.evm.datasize(metadata)

define i256 @runtime() {
  %lib = call i256 @llvm.evm.linkersymbol(metadata !2)
  %datasize = tail call i256 @llvm.evm.datasize(metadata !1)
  %res = add i256 %lib, %datasize
  ret i256 %res
}
!1 = !{!"A_38_deployed"}
!2 = !{!"library_id"}
