target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
target triple = "evm"

define i256 @__addmod(i256 %arg1, i256 %arg2, i256 %modulo) #0 {
  %res = call i256 @llvm.evm.addmod(i256 %arg1, i256 %arg2, i256 %modulo)
  ret i256 %res
}

define i256 @__mulmod(i256 %arg1, i256 %arg2, i256 %modulo) #0 {
  %res = call i256 @llvm.evm.mulmod(i256 %arg1, i256 %arg2, i256 %modulo)
  ret i256 %res
}

define i256 @__signextend(i256 %bytesize, i256 %val) #0 {
  %res = call i256 @llvm.evm.signextend(i256 %bytesize, i256 %val)
  ret i256 %res
}

define i256 @__exp(i256 %base, i256 %exp) #0 {
  %res = call i256 @llvm.evm.exp(i256 %base, i256 %exp)
  ret i256 %res
}

define i256 @__byte(i256 %index, i256 %val) #0 {
  %res = call i256 @llvm.evm.byte(i256 %index, i256 %val)
  ret i256 %res
}

define i256 @__sdiv(i256 %a, i256 %b) #0 {
  %res = call i256 @llvm.evm.sdiv(i256 %a, i256 %b)
  ret i256 %res
}

define i256 @__div(i256 %a, i256 %b) #0 {
  %res = call i256 @llvm.evm.div(i256 %a, i256 %b)
  ret i256 %res
}

define i256 @__smod(i256 %val, i256 %mod) #0 {
  %res = call i256 @llvm.evm.smod(i256 %val, i256 %mod)
  ret i256 %res
}

define i256 @__mod(i256 %val, i256 %mod) #0 {
  %res = call i256 @llvm.evm.mod(i256 %val, i256 %mod)
  ret i256 %res
}

define i256 @__shl(i256 %shift, i256 %val) #0 {
  %res = call i256 @llvm.evm.shl(i256 %shift, i256 %val)
  ret i256 %res
}

define i256 @__shr(i256 %shift, i256 %val) #0 {
  %res = call i256 @llvm.evm.shr(i256 %shift, i256 %val)
  ret i256 %res
}

define i256 @__sar(i256 %shift, i256 %val) #0 {
  %res = call i256 @llvm.evm.sar(i256 %shift, i256 %val)
  ret i256 %res
}

define i256 @__sha3(ptr addrspace(1) %offset, i256 %len, i1 %unused) #1 {
  %res = call i256 @llvm.evm.sha3(ptr addrspace(1) %offset, i256 %len)
  ret i256 %res
}

declare i256 @llvm.evm.addmod(i256, i256, i256)
declare i256 @llvm.evm.mulmod(i256, i256, i256)
declare i256 @llvm.evm.signextend(i256, i256)
declare i256 @llvm.evm.exp(i256, i256)
declare i256 @llvm.evm.byte(i256, i256)
declare i256 @llvm.evm.sdiv(i256, i256)
declare i256 @llvm.evm.div(i256, i256)
declare i256 @llvm.evm.mod(i256, i256)
declare i256 @llvm.evm.smod(i256, i256)
declare i256 @llvm.evm.shl(i256, i256)
declare i256 @llvm.evm.shr(i256, i256)
declare i256 @llvm.evm.sar(i256, i256)
declare i256 @llvm.evm.sha3(ptr addrspace(1), i256)

attributes #0 = { alwaysinline mustprogress nofree norecurse nosync nounwind readnone willreturn }
attributes #1 = { alwaysinline argmemonly readonly nofree null_pointer_is_valid }
