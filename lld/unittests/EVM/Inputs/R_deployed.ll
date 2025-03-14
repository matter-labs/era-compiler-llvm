target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"
declare i256 @llvm.evm.loadimmutable(metadata)
declare i256 @llvm.evm.datasize(metadata)
declare i256 @llvm.evm.dataoffset(metadata)

define i256 @runtime() {
  %res = tail call i256 @llvm.evm.loadimmutable(metadata !1)
  ret i256 %res
}

define i256 @get_runtimecode() {
  %datasize = tail call i256 @llvm.evm.datasize(metadata !2)
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !2)
  %res = add i256 %datasize, %dataoffset
  ret i256 %res
}
define i256 @get_initcode() {
  %datasize = tail call i256 @llvm.evm.datasize(metadata !3)
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !3)
  %res = add i256 %datasize, %dataoffset
  ret i256 %res
}
!1 = !{!"53"}
!2 = !{!"A_38.A_38_deployed"}
!3 = !{!"D_51"}
