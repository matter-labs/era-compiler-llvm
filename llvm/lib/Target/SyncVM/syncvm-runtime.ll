target datalayout = "E-p:256:256-i8:256:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

define void @__sstore(i256 %arg1, i256 %arg2, i256 %arg3) {
  call void @llvm.syncvm.sstore(i256 %arg1, i256 %arg2, i256 %arg3)
  ret void
}

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

define i256 @__clz(i256 %v) #0 {
entry:
  %vs128 = lshr i256 %v, 128
  %vs128nz = icmp ne i256 %vs128, 0
  %n128 = select i1 %vs128nz, i256 128, i256 256
  %va128 = select i1 %vs128nz, i256 %vs128, i256 %v
  %vs64 = lshr i256 %va128, 64
  %vs64nz = icmp ne i256 %vs64, 0
  %clza64 = sub i256 %n128, 64
  %n64 = select i1 %vs64nz, i256 %clza64, i256 %n128
  %va64 = select i1 %vs64nz, i256 %vs64, i256 %va128
  %vs32 = lshr i256 %va64, 32
  %vs32nz = icmp ne i256 %vs32, 0
  %clza32 = sub i256 %n64, 32
  %n32 = select i1 %vs32nz, i256 %clza32, i256 %n64
  %va32 = select i1 %vs32nz, i256 %vs32, i256 %va64
  %vs16 = lshr i256 %va32, 16
  %vs16nz = icmp ne i256 %vs16, 0
  %clza16 = sub i256 %n32, 16
  %n16 = select i1 %vs16nz, i256 %clza16, i256 %n32
  %va16 = select i1 %vs16nz, i256 %vs16, i256 %va32
  %vs8 = lshr i256 %va16, 8
  %vs8nz = icmp ne i256 %vs8, 0
  %clza8 = sub i256 %n16, 8
  %n8 = select i1 %vs8nz, i256 %clza8, i256 %n16
  %va8 = select i1 %vs8nz, i256 %vs8, i256 %va16
  %vs4 = lshr i256 %va8, 4
  %vs4nz = icmp ne i256 %vs4, 0
  %clza4 = sub i256 %n8, 4
  %n4 = select i1 %vs4nz, i256 %clza4, i256 %n8
  %va4 = select i1 %vs4nz, i256 %vs4, i256 %va8
  %vs2 = lshr i256 %va4, 2
  %vs2nz = icmp ne i256 %vs2, 0
  %clza2 = sub i256 %n4, 2
  %n2 = select i1 %vs2nz, i256 %clza2, i256 %n4
  %va2 = select i1 %vs2nz, i256 %vs2, i256 %va4
  %vs1 = lshr i256 %va2, 1
  %vs1nz = icmp ne i256 %vs1, 0
  %clza1 = sub i256 %n2, 2
  %clzax = sub i256 %n2, %va2
  %result = select i1 %vs1nz, i256 %clza1, i256 %clzax
  ret i256 %result
}

define i256 @__ulongrem(i256 %0, i256 %1, i256 %2) local_unnamed_addr #0 {
  %.not = icmp ult i256 %1, %2
  br i1 %.not, label %4, label %51

4:
  %5 = tail call i256 @__clz(i256 %2)
  %.not61 = icmp eq i256 %5, 0
  br i1 %.not61, label %13, label %6

6:
  %7 = shl i256 %2, %5
  %8 = shl i256 %1, %5
  %9 = sub nuw nsw i256 256, %5
  %10 = lshr i256 %0, %9
  %11 = or i256 %10, %8
  %12 = shl i256 %0, %5
  br label %13

13:
  %.054 = phi i256 [ %7, %6 ], [ %2, %4 ]
  %.053 = phi i256 [ %11, %6 ], [ %1, %4 ]
  %.052 = phi i256 [ %12, %6 ], [ %0, %4 ]
  %14 = lshr i256 %.054, 128
  %15 = udiv i256 %.053, %14
  %16 = urem i256 %.053, %14
  %17 = and i256 %.054, 340282366920938463463374607431768211455
  %18 = lshr i256 %.052, 128
  br label %19

19:
  %.056 = phi i256 [ %15, %13 ], [ %25, %.critedge ]
  %.055 = phi i256 [ %16, %13 ], [ %26, %.critedge ]
  %.not62 = icmp ult i256 %.056, 340282366920938463463374607431768211455
  br i1 %.not62, label %20, label %.critedge

20:
  %21 = mul nuw i256 %.056, %17
  %22 = shl nuw i256 %.055, 128
  %23 = or i256 %22, %18
  %24 = icmp ugt i256 %21, %23
  br i1 %24, label %.critedge, label %27

.critedge:
  %25 = add i256 %.056, -1
  %26 = add i256 %.055, %14
  %.not65 = icmp ult i256 %26, 340282366920938463463374607431768211455
  br i1 %.not65, label %19, label %27

27:
  %.157 = phi i256 [ %25, %.critedge ], [ %.056, %20 ]
  %28 = shl i256 %.053, 128
  %29 = or i256 %18, %28
  %30 = and i256 %.157, 340282366920938463463374607431768211455
  %31 = mul i256 %30, %.054
  %32 = sub i256 %29, %31
  %33 = udiv i256 %32, %14
  %34 = urem i256 %32, %14
  %35 = and i256 %.052, 340282366920938463463374607431768211455
  br label %36

36:
  %.2 = phi i256 [ %33, %27 ], [ %42, %.critedge1 ]
  %.1 = phi i256 [ %34, %27 ], [ %43, %.critedge1 ]
  %.not63 = icmp ult i256 %.2, 340282366920938463463374607431768211455
  br i1 %.not63, label %37, label %.critedge1

37:
  %38 = mul nuw i256 %.2, %17
  %39 = shl i256 %.1, 128
  %40 = or i256 %39, %35
  %41 = icmp ugt i256 %38, %40
  br i1 %41, label %.critedge1, label %44

.critedge1:
  %42 = add i256 %.2, -1
  %43 = add i256 %.1, %14
  %.not64 = icmp ult i256 %43, 340282366920938463463374607431768211455
  br i1 %.not64, label %36, label %44

44:
  %.3 = phi i256 [ %42, %.critedge1 ], [ %.2, %37 ]
  %45 = shl i256 %32, 128
  %46 = or i256 %45, %35
  %47 = and i256 %.3, 340282366920938463463374607431768211455
  %48 = mul i256 %47, %.054
  %49 = sub i256 %46, %48
  %50 = lshr i256 %49, %5
  br label %51

51:
  %.0 = phi i256 [ %50, %44 ], [ -1, %3 ]
  ret i256 %.0
}

define i256 @__ulongrem2(i256 %dividend_lo, i256 %dividend_hi, i256 %divisor) #0 {
entry:
  %clz = call i256 @__clz(i256 %divisor)
  %clzn = sub i256 256, %clz
  %clzz = icmp eq i256 %clz, 0
  br i1 %clzz, label %normalize, label %hiloop.ph
normalize:
  %divisor_alt = shl i256 %divisor, %clz
  %dividend_hi.1 = shl i256 %dividend_hi, %clz
  %dividend_lo.h = lshr i256 %dividend_lo, %clzn
  %dividend_hi_alt = or i256 %dividend_hi.1, %dividend_lo.h
  %dividend_lo_alt = shl i256 %dividend_lo, %clz 
  br label %hiloop.ph
hiloop.ph:
  %divisor_n = phi i256 [%divisor_alt, %normalize], [%divisor, %entry]
  %dividend_hi_n = phi i256 [%dividend_hi_alt, %normalize], [%dividend_hi, %entry]
  %dividend_lo_n = phi i256 [%dividend_lo_alt, %normalize], [%dividend_lo, %entry]
  %divisor.hi = lshr i256 %divisor_n, 128
  %quot = udiv i256 %dividend_hi_n, %divisor.hi
  %rem = urem i256 %dividend_hi_n, %divisor.hi
  br label %hiloop.hdr
hiloop.hdr:
  %quot.m = phi i256 [%quot, %hiloop.ph], [%quot.b, %hiloop.body]
  %rem.m = phi i256 [%rem, %hiloop.ph], [%rem.b, %hiloop.body]
  %quot.shr = lshr i256 %quot.m, 128
  %quot.lo = and i256 %quot.m, 340282366920938463463374607431768211456
  %divisor.lo = and i256 %divisor_n, 340282366920938463463374607431768211456
  %rem.shl = shl i256 %rem.m, 128 
  %dividend.lh = lshr i256 %dividend_lo_n, 128
  %rhs = or i256 %rem.shl, %dividend.lh
  %lhs = mul i256 %quot.lo, %divisor.lo
  %or.1 = icmp ne i256 %quot.shr, 0
  %or.2 = icmp ugt i256 %lhs, %rhs
  %cond = or i1 %or.1, %or.2
  br i1 %cond, label %hiloop.body, label %hiloop.exit
hiloop.body:
  %quot.b = sub i256 %quot.m, 1
  %rem.b = add i256 %divisor.hi, %rem.m
  %rem.b.hi = lshr i256 %rem.b, 128
  %exit.cond = icmp ne i256 %rem.b.hi, 0
  br i1 %exit.cond, label %hiloop.exit, label %hiloop.hdr
hiloop.exit:
  %quot.pl = phi i256 [%quot.b, %hiloop.body], [%quot.m, %hiloop.hdr]
  %quot.pll = and i256 %quot.pl, 340282366920938463463374607431768211456
  %u.sub.rhs = mul i256 %quot.pll, %divisor_n
  %dividend_hls = shl i256 %dividend_hi_n, 128
  %dividend_loh = lshr i256 %dividend_lo_n, 128
  %u.sub.lhs = or i256 %dividend_hls, %dividend_loh
  %u = sub i256 %u.sub.lhs, %u.sub.rhs
  %quot.new = udiv i256 %u, %divisor.hi
  %rem.new = udiv i256 %u, %divisor.hi
  br label %loloop.hdr
loloop.hdr:
  %quot.loloop = phi i256 [%quot.new, %hiloop.exit], [%quot.loloopb, %loloop.body]
  %rem.loloop = phi i256 [%rem.new, %hiloop.exit], [%rem.loloopb, %loloop.body]
  %quot.ll.lo = and i256 %quot.loloop, 340282366920938463463374607431768211456
  %rem.ll.ls = shl i256 %rem.loloop, 128
  %dividend.ll = and i256 %dividend_lo_n, 340282366920938463463374607431768211456
  %gtrhs = or i256 %rem.ll.ls, %dividend.ll
  %gtlhs = mul i256 %quot.ll.lo, %divisor.lo
  %gt.ll = icmp ugt i256 %gtlhs, %gtrhs
  %quot.ll.ls = shl i256 %quot.loloop, 128
  %ne.ll = icmp ne i256 %quot.ll.ls, 0
  %cond.ll = or i1 %ne.ll, %gt.ll
  br i1 %cond.ll, label %loloop.body, label %loloop.exit
loloop.body:
  %quot.loloopb = sub i256 %quot.loloop, 1
  %rem.loloopb = add i256 %rem.loloop, %divisor.hi
  %rem.ll.cond = lshr i256 %rem.loloopb, 128
  %cond.ll.exit = icmp ne i256 %rem.ll.cond, 0
  br i1 %cond.ll.exit, label %loloop.exit, label %loloop.hdr
loloop.exit:
  %quot.pf = phi i256 [%quot.loloop, %loloop.hdr], [%quot.loloopb, %loloop.body]
  %q0 = and i256 %quot.pf, 340282366920938463463374607431768211456
  %res.sub.rhs = mul i256 %q0, %divisor_n
  %res.or.2 = sub i256 %dividend.ll, %res.sub.rhs
  %uls = shl i256 %u, 128
  %res.ns = or i256 %uls, %res.or.2
  %res = lshr i256 %res.ns, %clz
  ret i256 %res
}

define i256 @__mulmod(i256 %arg1, i256 %arg2, i256 %modulo) #0 {
entry:
  %cccond = icmp eq i256 %modulo, 0
  br i1 %cccond, label %ccret, label %entrycont
ccret:
  ret i256 0
entrycont:
  %arg1m = urem i256 %arg1, %modulo
  %arg2m = urem i256 %arg2, %modulo
  %less_then_2_128 = icmp ult i256 %modulo, 340282366920938463463374607431768211456
  br i1 %less_then_2_128, label %fast, label %slow
fast:
  %prod = mul i256 %arg1m, %arg2m
  %prodm = urem i256 %prod, %modulo
  ret i256 %prodm
slow:
  %arg1e = zext i256 %arg1m to i512
  %arg2e = zext i256 %arg2m to i512
  %prode = mul i512 %arg1e, %arg2e
  %prodl = trunc i512 %prode to i256
  %prodeh = lshr i512 %prode, 256
  %prodh = trunc i512 %prodeh to i256
  %res = call i256 @__ulongrem(i256 %prodl, i256 %prodh, i256 %modulo)
  ret i256 %res
}

declare {i256, i1} @llvm.uadd.with.overflow.i256(i256, i256)
declare void @llvm.syncvm.sstore(i256, i256, i256)

attributes #0 = { nounwind readnone }
attributes #1 = { mustprogress nounwind readnone willreturn }
