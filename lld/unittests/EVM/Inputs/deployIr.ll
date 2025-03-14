target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"
declare i256 @llvm.evm.datasize(metadata)
declare i256 @llvm.evm.dataoffset(metadata)
declare i256 @llvm.evm.linkersymbol(metadata)

define i256 @foo() {
  %res = call i256 @llvm.evm.linkersymbol(metadata !1)
  ret i256 %res
}

define i256 @bar() {
  %linkersym = call i256 @llvm.evm.linkersymbol(metadata !1)
  %datasize = tail call i256 @llvm.evm.datasize(metadata !2)
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !2)
  %tmp = add i256 %datasize, %dataoffset
  %res = add i256 %tmp, %linkersym
  ret i256 %res
}
!1 = !{!"library_id"}
!2 = !{!"Test_26_deployed"}
