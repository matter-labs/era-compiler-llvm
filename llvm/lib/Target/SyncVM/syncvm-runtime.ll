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

define i256 @__small_load_as0(i256 %addr, i256 %size_in_bits) {
entry:
  %offset_lead_bytes = urem i256 %addr, 32
  %offset_lead_bits = mul nuw nsw i256 %offset_lead_bytes, 8
  %base_int = sub i256 %addr, %offset_lead_bytes
  %base_ptr = inttoptr i256 %base_int to i256*
  %hival = load i256, i256* %base_ptr, align 32
  %offset_size = add nuw nsw i256 %offset_lead_bits, %size_in_bits
  %fits_cell = icmp ule i256 %offset_size, 256
  %inv_size = sub i256 256, %size_in_bits
  br i1 %fits_cell, label %one_cell, label %two_cells
one_cell:
  %offset_size_inv = sub nsw nuw i256 256, %offset_size
  %val_shifted = lshr i256 %hival, %offset_size_inv
  %mask_one = lshr i256 -1, %inv_size
  %one_cell_res = and i256 %mask_one, %val_shifted
  ret i256 %one_cell_res
two_cells:
  %hi_bits = sub nuw nsw i256 256, %offset_lead_bits
  %lo_bits = sub nuw nsw i256 %size_in_bits, %hi_bits
  %lo_bits_inv = sub nsw nuw i256 256, %lo_bits
  %hival_shifted = shl i256 %hival, %lo_bits
  %lo_base_int = add nsw nuw i256 %base_int, 32
  %lo_base_ptr = inttoptr i256 %lo_base_int to i256*
  %loval = load i256, i256* %lo_base_ptr, align 32
  %loval_shifted = lshr i256 %loval, %lo_bits_inv
  %valcomb = or i256 %loval_shifted, %hival_shifted
  %size_in_bits_inv = sub i256 256, %size_in_bits
  %mask_two = lshr i256 -1, %size_in_bits_inv
  %two_cells_res = and i256 %mask_two, %valcomb
  ret i256 %two_cells_res
}

define void @__small_store_as0(i256 %addr, i256 %size_in_bits, i256 %value) {
entry:
  %offset_lead_bytes = urem i256 %addr, 32
  %offset_lead_bits = mul nuw nsw i256 %offset_lead_bytes, 8
  %offset_lead_bits_inv = sub nsw nuw i256 256, %offset_lead_bits
  %base_int = sub i256 %addr, %offset_lead_bytes
  %base_ptr = inttoptr i256 %base_int to i256*
  %hival_orig = load i256, i256* %base_ptr, align 32
  %offset_size = add nuw nsw i256 %offset_lead_bits, %size_in_bits
  %fits_cell = icmp ule i256 %offset_size, 256
  %inv_size = sub i256 256, %size_in_bits
  %mask_hi_common.1 = shl i256 -1, %offset_lead_bits_inv
  %has_nlz = icmp eq i256 %offset_lead_bits, 0
  %mask_hi_common = select i1 %has_nlz, i256 0, i256 %mask_hi_common.1
  br i1 %fits_cell, label %one_cell, label %two_cells
one_cell:
  %trailing_onecell = sub i256 %offset_lead_bits_inv, %size_in_bits
  %has_trailing_bits = icmp ugt i256 %trailing_onecell, 0
  br i1 %has_trailing_bits, label %one_cell_trail, label %one_cell_common
one_cell_trail:
  %store_oc_shifted = shl i256 %value, %trailing_onecell
  %trailing_onecell_inv = sub nsw nuw i256 256, %trailing_onecell
  %mask_oc_lo = lshr i256 -1, %trailing_onecell_inv
  %mask_oc_trail = or i256 %mask_hi_common, %mask_oc_lo
  br label %one_cell_common
one_cell_common:
  %store_oc = phi i256 [%value, %one_cell], [%store_oc_shifted, %one_cell_trail]
  %mask_oc = phi i256 [%mask_hi_common, %one_cell], [%mask_oc_trail, %one_cell_trail]
  %orig_oc_masked = and i256 %mask_oc, %hival_orig
  %store_oc.f = or i256 %orig_oc_masked, %store_oc
  store i256 %store_oc.f, i256* %base_ptr
  ret void
two_cells:
  %hi_orig = and i256 %hival_orig, %mask_hi_common
  %bits_outstanding.1 = add nsw nuw i256 %size_in_bits, %offset_lead_bits
  %bits_outstanding = sub nsw nuw i256 %bits_outstanding.1, 256
  %bits_outstanding_inv = sub nsw nuw i256 256, %bits_outstanding.1
  %hi_val_outstanding = lshr i256 %value, %bits_outstanding
  %hi_store = or i256 %hi_val_outstanding, %hi_orig
  store i256 %hi_store, i256* %base_ptr, align 32
  %lo_base_int = add nsw nuw i256 %base_int, 32
  %lo_base_ptr = inttoptr i256 %lo_base_int to i256*
  %loval_orig = load i256, i256* %lo_base_ptr, align 32
  %lo_store = shl i256 %value, %bits_outstanding_inv
  %loval_mask = lshr i256 -1, %bits_outstanding
  %loval_masked = and i256 %loval_mask, %loval_orig
  %loval_store = or i256 %loval_masked, %lo_store
  store i256 %loval_store, i256* %lo_base_ptr, align 32
  ret void
}

define void @__cxa_throw(i8* %addr, i8*, i8*) {
  %addrval = ptrtoint i8* %addr to i256
  call void @llvm.syncvm.throw(i256 %addrval)
  unreachable
}

define i256 @__signextend(i256 %numbyte, i256 %value) #1 {
  %numbit_byte = mul nuw nsw i256 %numbyte, 8
  %numbit = add nsw nuw i256 %numbit_byte, 7
  %numbit_inv = sub i256 256, %numbit
  %signmask = shl i256 1, %numbit
  %valmask = lshr i256 -1, %numbit_inv
  %ext1 = shl i256 -1, %numbit
  %signv = and i256 %signmask, %value
  %sign = icmp ne i256 %signv, 0
  %valclean = and i256 %value, %valmask
  %sext = select i1 %sign, i256 %ext1, i256 0
  %result = or i256 %sext, %valclean
  ret i256 %result
}

define {i256, i1}* @__farcall(i256 %abi_params, i256 %address, {i256, i1}* %in_res) personality i32 ()* @__personality {
entry:
  %in_res_result_ptr = getelementptr {i256, i1}, {i256, i1}* %in_res, i32 0, i32 0
  %in_res_flag_ptr = getelementptr {i256, i1}, {i256, i1}* %in_res, i32 0, i32 1
  %invoke_res = invoke i256 @__farcall_int(i256 %abi_params, i256 %address)
    to label %ok unwind label %err
ok:
  store i256 %invoke_res, i256* %in_res_result_ptr
  store i1 1, i1* %in_res_flag_ptr
  ret {i256, i1}* %in_res

err:
  %res.2 = landingpad {i256, i256} cleanup
  %res = extractvalue {i256, i256} %res.2, 0
  %flag = call i256 @llvm.syncvm.iflt(i256 1, i256 0)
  %is_set = trunc i256 %flag to i1
  br i1 %is_set, label %revert, label %err_ok

revert:
  call void @llvm.syncvm.throw(i256 %res)
  unreachable

err_ok:
  store i256 %res, i256* %in_res_result_ptr
  store i1 0, i1* %in_res_flag_ptr
  ret {i256, i1}* %in_res
}

define {i256, i1}* @__staticcall(i256 %abi_params, i256 %address, {i256, i1}* %in_res) personality i32 ()* @__personality {
entry:
  %in_res_result_ptr = getelementptr {i256, i1}, {i256, i1}* %in_res, i32 0, i32 0
  %in_res_flag_ptr = getelementptr {i256, i1}, {i256, i1}* %in_res, i32 0, i32 1
  %invoke_res = invoke i256 @__staticcall_int(i256 %abi_params, i256 %address)
    to label %ok unwind label %err
ok:
  store i256 %invoke_res, i256* %in_res_result_ptr
  store i1 1, i1* %in_res_flag_ptr
  ret {i256, i1}* %in_res

err:
  %res.2 = landingpad {i256, i256} cleanup
  %res = extractvalue {i256, i256} %res.2, 0
  %flag = call i256 @llvm.syncvm.iflt(i256 1, i256 0)
  %is_set = trunc i256 %flag to i1
  br i1 %is_set, label %revert, label %err_ok

revert:
  call void @llvm.syncvm.throw(i256 %res)
  unreachable

err_ok:
  store i256 %res, i256* %in_res_result_ptr
  store i1 0, i1* %in_res_flag_ptr
  ret {i256, i1}* %in_res
}

define {i256, i1}* @__delegatecall(i256 %abi_params, i256 %address, {i256, i1}* %in_res) personality i32 ()* @__personality {
entry:
  %in_res_result_ptr = getelementptr {i256, i1}, {i256, i1}* %in_res, i32 0, i32 0
  %in_res_flag_ptr = getelementptr {i256, i1}, {i256, i1}* %in_res, i32 0, i32 1
  %invoke_res = invoke i256 @__delegatecall_int(i256 %abi_params, i256 %address)
    to label %ok unwind label %err
ok:
  store i256 %invoke_res, i256* %in_res_result_ptr
  store i1 1, i1* %in_res_flag_ptr
  ret {i256, i1}* %in_res

err:
  %res.2 = landingpad {i256, i256} cleanup
  %res = extractvalue {i256, i256} %res.2, 0
  %flag = call i256 @llvm.syncvm.iflt(i256 1, i256 0)
  %is_set = trunc i256 %flag to i1
  br i1 %is_set, label %revert, label %err_ok

revert:
  call void @llvm.syncvm.throw(i256 %res)
  unreachable

err_ok:
  store i256 %res, i256* %in_res_result_ptr
  store i1 0, i1* %in_res_flag_ptr
  ret {i256, i1}* %in_res
}

define {i256, i1}* @__mimiccall(i256 %abi_params, i256 %address, i256 %mimic, {i256, i1}* %in_res) personality i32 ()* @__personality {
entry:
  %in_res_result_ptr = getelementptr {i256, i1}, {i256, i1}* %in_res, i32 0, i32 0
  %in_res_flag_ptr = getelementptr {i256, i1}, {i256, i1}* %in_res, i32 0, i32 1
  %invoke_res = invoke i256 @__mimiccall_int(i256 %abi_params, i256 %address, i256 %mimic)
    to label %ok unwind label %err
ok:
  store i256 %invoke_res, i256* %in_res_result_ptr
  store i1 1, i1* %in_res_flag_ptr
  ret {i256, i1}* %in_res

err:
  %res.2 = landingpad {i256, i256} cleanup
  %res = extractvalue {i256, i256} %res.2, 0
  %flag = call i256 @llvm.syncvm.iflt(i256 1, i256 0)
  %is_set = trunc i256 %flag to i1
  br i1 %is_set, label %revert, label %err_ok

revert:
  call void @llvm.syncvm.throw(i256 %res)
  unreachable

err_ok:
  store i256 %res, i256* %in_res_result_ptr
  store i1 0, i1* %in_res_flag_ptr
  ret {i256, i1}* %in_res
}

define void @__sstore(i256 %val, i256 %key) {
  call void @llvm.syncvm.sstore(i256 %key, i256 %val)
  ret void
}

define i256 @__sload(i256 %key) {
  %res = call i256 @llvm.syncvm.sload(i256 %key)
  ret i256 %res
}

define void @__small_store_as1(i256 %addr.i, i256 %value, i256 %size_in_bits) nounwind {
  %addr = inttoptr i256 %addr.i to i256 addrspace(1)*
  %sizeinv = sub nsw nuw i256 256, %size_in_bits
  %maskload = lshr i256 -1, %size_in_bits
  %maskval = shl i256 -1, %sizeinv
  %val.m = and i256 %value, %maskval
  %oldval = load i256, i256 addrspace(1)* %addr, align 1
  %oldval.m = and i256 %oldval, %maskload
  %val.f = or i256 %val.m, %oldval.m
  store i256 %val.f, i256 addrspace(1)* %addr, align 1
  ret void
}

define void @__small_store_as2(i256 %addr.i, i256 %value, i256 %size_in_bits) nounwind {
  %addr = inttoptr i256 %addr.i to i256 addrspace(2)*
  %sizeinv = sub nsw nuw i256 256, %size_in_bits
  %maskload = lshr i256 -1, %size_in_bits
  %maskval = shl i256 -1, %sizeinv
  %val.m = and i256 %value, %maskval
  %oldval = load i256, i256 addrspace(2)* %addr, align 1
  %oldval.m = and i256 %oldval, %maskload
  %val.f = or i256 %val.m, %oldval.m
  store i256 %val.f, i256 addrspace(2)* %addr, align 1
  ret void
}

define void @__memset_uma_as1(i256 addrspace(1)* %dest, i256 %val, i256 %size) nounwind {
entry:
  %numcells = udiv i256 %size, 32
  %hascells = icmp ugt i256 %numcells, 0
  %dest.int = ptrtoint i256 addrspace(1)* %dest to i256
  br i1 %hascells, label %copycells, label %copybytes
copycells:
  %cellsrem = phi i256 [%numcells, %entry], [%cellsrem.next, %copycells]
  %currentdest.int = phi i256 [%dest.int, %entry], [%currentdest.inext, %copycells]
  %currentdest = inttoptr i256 %currentdest.int to i256 addrspace(1)*
  store i256 %val, i256 addrspace(1)* %currentdest, align 1
  %currentdest.inext = add nsw nuw i256 %currentdest.int, 32
  %cellsrem.next = sub nsw nuw i256 %cellsrem, 1
  %continue = icmp ne i256 %cellsrem.next, 0
  br i1 %continue, label %copycells, label %copybytes
copybytes:
  %addr.int = phi i256 [%dest.int, %entry], [%currentdest.inext, %copycells]
  %rembytes = urem i256 %size, 32
  %need.residual.copy = icmp ne i256 %rembytes, 0
  br i1 %need.residual.copy, label %residual, label %return
residual:
  %rembits = mul nsw nuw i256 %rembytes, 8
  call void @__small_store_as1(i256 %addr.int, i256 %val, i256 %rembits)
  ret void
return:
  ret void
}

define void @__memset_uma_as2(i256 addrspace(2)* %dest, i256 %val, i256 %size) nounwind {
entry:
  %numcells = udiv i256 %size, 32
  %hascells = icmp ugt i256 %numcells, 0
  %dest.int = ptrtoint i256 addrspace(2)* %dest to i256
  br i1 %hascells, label %copycells, label %copybytes
copycells:
  %cellsrem = phi i256 [%numcells, %entry], [%cellsrem.next, %copycells]
  %currentdest.int = phi i256 [%dest.int, %entry], [%currentdest.inext, %copycells]
  %currentdest = inttoptr i256 %currentdest.int to i256 addrspace(2)*
  store i256 %val, i256 addrspace(2)* %currentdest, align 1
  %currentdest.inext = add nsw nuw i256 %currentdest.int, 32
  %cellsrem.next = sub nsw nuw i256 %cellsrem, 1
  %continue = icmp ne i256 %cellsrem.next, 0
  br i1 %continue, label %copycells, label %copybytes
copybytes:
  %addr.int = phi i256 [%dest.int, %entry], [%currentdest.inext, %copycells]
  %rembytes = urem i256 %size, 32
  %need.residual.copy = icmp ne i256 %rembytes, 0
  br i1 %need.residual.copy, label %residual, label %return
residual:
  %rembits = mul nsw nuw i256 %rembytes, 8
  call void @__small_store_as2(i256 %addr.int, i256 %val, i256 %rembits)
  ret void
return:
  ret void
}

declare {i256, i1} @llvm.uadd.with.overflow.i256(i256, i256)
declare void @llvm.syncvm.throw(i256)
declare void @llvm.syncvm.sstore(i256 %key, i256 %val)
declare i256 @llvm.syncvm.sload(i256 %key)
declare i256 @llvm.syncvm.iflt(i256, i256)
declare i256 @__farcall_int(i256, i256)
declare i256 @__staticcall_int(i256, i256)
declare i256 @__delegatecall_int(i256, i256)
declare i256 @__mimiccall_int(i256, i256, i256)
declare i32 @__personality()

attributes #0 = { nounwind readnone }
attributes #1 = { mustprogress nounwind readnone willreturn }
