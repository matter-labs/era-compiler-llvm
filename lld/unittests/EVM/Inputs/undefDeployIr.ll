target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"
declare i256 @llvm.evm.datasize(metadata)
declare i256 @llvm.evm.dataoffset(metadata)
declare i256 @llvm.evm.linkersymbol(metadata)

define i256 @bar() {
  %datasize = tail call i256 @llvm.evm.datasize(metadata !1)
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !1)
  %res = add i256 %datasize, %dataoffset
  ret i256 %res
}
!1 = !{!"unknown"}
