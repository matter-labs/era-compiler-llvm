target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

define i256 @__addmod(i256 %arg1, i256 %arg2, i256 %modulo) #0 {
entry:
  %is_zero = icmp eq i256 %modulo, 0
  br i1 %is_zero, label %return, label %addmod

addmod:
  %arg1m = urem i256 %arg1, %modulo
  %arg2m = urem i256 %arg2, %modulo
  %res = call {i256, i1} @llvm.uadd.with.overflow.i256(i256 %arg1m, i256 %arg2m)
  %sum = extractvalue {i256, i1} %res, 0
  %obit = extractvalue {i256, i1} %res, 1
  %sum.mod = urem i256 %sum, %modulo
  br i1 %obit, label %overflow, label %return

overflow:
  %mod.inv = xor i256 %modulo, -1
  %sum1 = add i256 %sum, %mod.inv
  %sum.ovf = add i256 %sum1, 1
  br label %return

return:
  %value = phi i256 [0, %entry], [%sum.mod, %addmod], [%sum.ovf, %overflow]
  ret i256 %value
}

define i256 @__mulmod(i256 %arg1, i256 %arg2, i256 %modulo) #0 {
entry:
  %cond = icmp ult i256 %modulo, 340282366920938463463374607431768211456
  br i1 %cond, label %fast, label %slow
fast:
  %arg1m = urem i256 %arg1, %modulo
  %arg2m = urem i256 %arg2, %modulo
  %mul.res = mul nuw i256 %arg1m, %arg2m
  %mulm = urem i256 %mul.res, %modulo
  ret i256 %mulm
slow:
  ret i256 0
}

declare {i256, i1} @llvm.uadd.with.overflow.i256(i256, i256)

attributes #0 = { nounwind }
