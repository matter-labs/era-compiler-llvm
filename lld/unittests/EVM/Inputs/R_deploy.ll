target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"
declare i256 @llvm.evm.datasize(metadata)
declare i256 @llvm.evm.dataoffset(metadata)
declare i256 @llvm.evm.codesize()

define i256 @init() {
  %datasize = tail call i256 @llvm.evm.datasize(metadata !1)
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !1)
  %res = add i256 %datasize, %dataoffset
  ret i256 %res
}
define i256 @A_init() {
  %datasize = tail call i256 @llvm.evm.datasize(metadata !4)
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !4)
  %res = add i256 %datasize, %dataoffset
  ret i256 %res
}
define i256 @args_len() {
  %datasize = tail call i256 @llvm.evm.datasize(metadata !2)
  %codesize = tail call i256 @llvm.evm.codesize()
  %res = sub i256 %codesize, %datasize
  ret i256 %res
}
define i256 @A_runtime() {
  %datasize = tail call i256 @llvm.evm.datasize(metadata !3)
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !3)
  %res = add i256 %datasize, %dataoffset
  ret i256 %res
}
!1 = !{!"R_107_deployed"}
!2 = !{!"R_107"}
!3 = !{!"A_38.A_38_deployed"}
!4 = !{!"A_38"}
