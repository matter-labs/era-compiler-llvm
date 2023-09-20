target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "eravm"

define i256 @__small_load_as0(i256 %addr, i256 %size_in_bits) local_unnamed_addr #0 {
entry:
  %offset_lead_bytes = shl i256 %addr, 3
  %offset_lead_bits = and i256 %offset_lead_bytes, 248
  %base_int = and i256 %addr, -32
  %base_ptr = inttoptr i256 %base_int to i256*
  %hival = load i256, i256* %base_ptr, align 32
  %offset_size = add nuw nsw i256 %offset_lead_bits, %size_in_bits
  %fits_cell = icmp ult i256 %offset_size, 257
  br i1 %fits_cell, label %one_cell, label %two_cells

common.ret:
  %valcomb.sink = phi i256 [ %valcomb, %two_cells ], [ %val_shifted, %one_cell ]
  %inv_size = sub i256 256, %size_in_bits
  %mask_two = lshr i256 -1, %inv_size
  %two_cells_res = and i256 %valcomb.sink, %mask_two
  ret i256 %two_cells_res

one_cell:
  %offset_size_inv = sub nuw nsw i256 256, %offset_size
  %val_shifted = lshr i256 %hival, %offset_size_inv
  br label %common.ret

two_cells:
  %hi_bits.neg = or i256 %offset_lead_bytes, -256
  %lo_bits = add i256 %hi_bits.neg, %size_in_bits
  %lo_bits_inv = sub nuw nsw i256 256, %lo_bits
  %hival_shifted = shl i256 %hival, %lo_bits
  %lo_base_int = add nuw nsw i256 %base_int, 32
  %lo_base_ptr = inttoptr i256 %lo_base_int to i256*
  %loval = load i256, i256* %lo_base_ptr, align 32
  %loval_shifted = lshr i256 %loval, %lo_bits_inv
  %valcomb = or i256 %loval_shifted, %hival_shifted
  br label %common.ret
}

define void @__small_store_as0(i256 %addr, i256 %size_in_bits, i256 %value) local_unnamed_addr #1 {
entry:
  %offset_lead_bytes = and i256 %addr, 31
  %offset_lead_bits = shl nuw nsw i256 %offset_lead_bytes, 3
  %offset_lead_bits_inv = sub nuw nsw i256 256, %offset_lead_bits
  %base_int = and i256 %addr, -32
  %base_ptr = inttoptr i256 %base_int to i256*
  %hival_orig = load i256, i256* %base_ptr, align 32
  %offset_size = add nuw nsw i256 %offset_lead_bits, %size_in_bits
  %fits_cell = icmp ult i256 %offset_size, 257
  %mask_hi_common.1 = shl nsw i256 -1, %offset_lead_bits_inv
  %has_nlz = icmp eq i256 %offset_lead_bytes, 0
  %mask_hi_common = select i1 %has_nlz, i256 0, i256 %mask_hi_common.1
  br i1 %fits_cell, label %one_cell, label %two_cells

one_cell:
  %trailing_onecell = sub i256 %offset_lead_bits_inv, %size_in_bits
  %has_trailing_bits.not = icmp eq i256 %offset_lead_bits_inv, %size_in_bits
  %trailing_onecell_inv = sub nuw nsw i256 256, %trailing_onecell
  %mask_oc_lo = lshr i256 -1, %trailing_onecell_inv
  %store_oc_shifted = select i1 %has_trailing_bits.not, i256 0, i256 %trailing_onecell
  %store_oc = shl i256 %value, %store_oc_shifted
  %mask_oc_trail = select i1 %has_trailing_bits.not, i256 0, i256 %mask_oc_lo
  %mask_oc = or i256 %mask_oc_trail, %mask_hi_common
  %orig_oc_masked = and i256 %hival_orig, %mask_oc
  %store_oc.f = or i256 %orig_oc_masked, %store_oc
  store i256 %store_oc.f, i256* %base_ptr, align 32
  br label %common.ret

common.ret:
  ret void

two_cells:
  %hi_orig = and i256 %hival_orig, %mask_hi_common
  %bits_outstanding = add nsw i256 %offset_size, -256
  %bits_outstanding_inv = sub nuw nsw i256 256, %offset_size
  %hi_val_outstanding = lshr i256 %value, %bits_outstanding
  %hi_store = or i256 %hi_orig, %hi_val_outstanding
  store i256 %hi_store, i256* %base_ptr, align 32
  %lo_base_int = add nuw nsw i256 %base_int, 32
  %lo_base_ptr = inttoptr i256 %lo_base_int to i256*
  %loval_orig = load i256, i256* %lo_base_ptr, align 32
  %lo_store = shl nuw nsw i256 %value, %bits_outstanding_inv
  %loval_mask = lshr i256 -1, %bits_outstanding
  %loval_masked = and i256 %loval_orig, %loval_mask
  %loval_store = or i256 %loval_masked, %lo_store
  store i256 %loval_store, i256* %lo_base_ptr, align 32
  br label %common.ret
}

define void @__small_store_as1(i256 %addr.i, i256 %value, i256 %size_in_bits) local_unnamed_addr #1 {
  %addr = inttoptr i256 %addr.i to i256 addrspace(1)*
  %sizeinv = sub nuw nsw i256 256, %size_in_bits
  %maskload = lshr i256 -1, %size_in_bits
  %maskval = shl nsw i256 -1, %sizeinv
  %val.m = and i256 %maskval, %value
  %oldval = load i256, i256 addrspace(1)* %addr, align 1
  %oldval.m = and i256 %oldval, %maskload
  %val.f = or i256 %oldval.m, %val.m
  store i256 %val.f, i256 addrspace(1)* %addr, align 1
  ret void
}

define void @__small_store_as2(i256 %addr.i, i256 %value, i256 %size_in_bits) local_unnamed_addr #1 {
  %addr = inttoptr i256 %addr.i to i256 addrspace(2)*
  %sizeinv = sub nuw nsw i256 256, %size_in_bits
  %maskload = lshr i256 -1, %size_in_bits
  %maskval = shl nsw i256 -1, %sizeinv
  %val.m = and i256 %maskval, %value
  %oldval = load i256, i256 addrspace(2)* %addr, align 1
  %oldval.m = and i256 %oldval, %maskload
  %val.f = or i256 %oldval.m, %val.m
  store i256 %val.f, i256 addrspace(2)* %addr, align 1
  ret void
}

define void @__memset_uma_as1(i256 addrspace(1)* %dest, i256 %val, i256 %size) local_unnamed_addr #2 {
entry:
  %hascells.not = icmp ult i256 %size, 32
  %dest.int = ptrtoint i256 addrspace(1)* %dest to i256
  br i1 %hascells.not, label %copybytes, label %copycells.preheader

copycells.preheader:
  %numcells1 = lshr i256 %size, 5
  br label %copycells

copycells:
  %cellsrem = phi i256 [ %cellsrem.next, %copycells ], [ %numcells1, %copycells.preheader ]
  %currentdest.int = phi i256 [ %currentdest.inext, %copycells ], [ %dest.int, %copycells.preheader ]
  %currentdest = inttoptr i256 %currentdest.int to i256 addrspace(1)*
  store i256 %val, i256 addrspace(1)* %currentdest, align 1
  %currentdest.inext = add nuw nsw i256 %currentdest.int, 32
  %cellsrem.next = add nsw i256 %cellsrem, -1
  %continue.not = icmp eq i256 %cellsrem.next, 0
  br i1 %continue.not, label %copybytes, label %copycells

copybytes:
  %addr.int = phi i256 [ %dest.int, %entry ], [ %currentdest.inext, %copycells ]
  %rembytes = and i256 %size, 31
  %need.residual.copy.not = icmp eq i256 %rembytes, 0
  br i1 %need.residual.copy.not, label %common.ret, label %residual

common.ret:
  ret void

residual:
  %rembits = shl nuw nsw i256 %rembytes, 3
  %addr.i = inttoptr i256 %addr.int to i256 addrspace(1)*
  %sizeinv.i = sub nuw nsw i256 256, %rembits
  %maskload.i = lshr i256 -1, %rembits
  %maskval.i = shl nsw i256 -1, %sizeinv.i
  %val.m.i = and i256 %maskval.i, %val
  %oldval.i = load i256, i256 addrspace(1)* %addr.i, align 1
  %oldval.m.i = and i256 %oldval.i, %maskload.i
  %val.f.i = or i256 %oldval.m.i, %val.m.i
  store i256 %val.f.i, i256 addrspace(1)* %addr.i, align 1
  br label %common.ret
}

define void @__memset_uma_as2(i256 addrspace(2)* %dest, i256 %val, i256 %size) local_unnamed_addr #2 {
entry:
  %hascells.not = icmp ult i256 %size, 32
  %dest.int = ptrtoint i256 addrspace(2)* %dest to i256
  br i1 %hascells.not, label %copybytes, label %copycells.preheader

copycells.preheader:
  %numcells1 = lshr i256 %size, 5
  br label %copycells

copycells:
  %cellsrem = phi i256 [ %cellsrem.next, %copycells ], [ %numcells1, %copycells.preheader ]
  %currentdest.int = phi i256 [ %currentdest.inext, %copycells ], [ %dest.int, %copycells.preheader ]
  %currentdest = inttoptr i256 %currentdest.int to i256 addrspace(2)*
  store i256 %val, i256 addrspace(2)* %currentdest, align 1
  %currentdest.inext = add nuw nsw i256 %currentdest.int, 32
  %cellsrem.next = add nsw i256 %cellsrem, -1
  %continue.not = icmp eq i256 %cellsrem.next, 0
  br i1 %continue.not, label %copybytes, label %copycells

copybytes:
  %addr.int = phi i256 [ %dest.int, %entry ], [ %currentdest.inext, %copycells ]
  %rembytes = and i256 %size, 31
  %need.residual.copy.not = icmp eq i256 %rembytes, 0
  br i1 %need.residual.copy.not, label %common.ret, label %residual

common.ret:
  ret void

residual:
  %rembits = shl nuw nsw i256 %rembytes, 3
  %addr.i = inttoptr i256 %addr.int to i256 addrspace(2)*
  %sizeinv.i = sub nuw nsw i256 256, %rembits
  %maskload.i = lshr i256 -1, %rembits
  %maskval.i = shl nsw i256 -1, %sizeinv.i
  %val.m.i = and i256 %maskval.i, %val
  %oldval.i = load i256, i256 addrspace(2)* %addr.i, align 1
  %oldval.m.i = and i256 %oldval.i, %maskload.i
  %val.f.i = or i256 %oldval.m.i, %val.m.i
  store i256 %val.f.i, i256 addrspace(2)* %addr.i, align 1
  br label %common.ret
}

define {i8 addrspace(3)*, i1} @__farcall(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12) #3 personality i32 ()* @__personality {
entry:
  %invoke_res = invoke i8 addrspace(3)* @__farcall_int(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12)
    to label %ok unwind label %err
ok:
  %res.1u = insertvalue {i8 addrspace(3)*, i1} undef, i8 addrspace(3)* %invoke_res, 0
  %res.1f = insertvalue {i8 addrspace(3)*, i1} %res.1u, i1 1, 1
  ret {i8 addrspace(3)*, i1} %res.1f

err:
  %res.2u = landingpad {i8 addrspace(3)*, i1} cleanup
  %res.2f = insertvalue {i8 addrspace(3)*, i1} %res.2u, i1 0, 1
  ret {i8 addrspace(3)*, i1} %res.2f
}

define {i8 addrspace(3)*, i1} @__staticcall(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12) #3 personality i32 ()* @__personality {
entry:
  %invoke_res = invoke i8 addrspace(3)* @__staticcall_int(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12)
    to label %ok unwind label %err
ok:
  %res.1u = insertvalue {i8 addrspace(3)*, i1} undef, i8 addrspace(3)* %invoke_res, 0
  %res.1f = insertvalue {i8 addrspace(3)*, i1} %res.1u, i1 1, 1
  ret {i8 addrspace(3)*, i1} %res.1f

err:
  %res.2u = landingpad {i8 addrspace(3)*, i1} cleanup
  %res.2f = insertvalue {i8 addrspace(3)*, i1} %res.2u, i1 0, 1
  ret {i8 addrspace(3)*, i1} %res.2f
}

define {i8 addrspace(3)*, i1} @__delegatecall(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12) #3 personality i32 ()* @__personality {
entry:
  %invoke_res = invoke i8 addrspace(3)* @__delegatecall_int(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12)
    to label %ok unwind label %err
ok:
  %res.1u = insertvalue {i8 addrspace(3)*, i1} undef, i8 addrspace(3)* %invoke_res, 0
  %res.1f = insertvalue {i8 addrspace(3)*, i1} %res.1u, i1 1, 1
  ret {i8 addrspace(3)*, i1} %res.1f

err:
  %res.2u = landingpad {i8 addrspace(3)*, i1} cleanup
  %res.2f = insertvalue {i8 addrspace(3)*, i1} %res.2u, i1 0, 1
  ret {i8 addrspace(3)*, i1} %res.2f
}

define {i8 addrspace(3)*, i1} @__mimiccall(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12, i256 %mimic) #3 personality i32 ()* @__personality {
entry:
  %invoke_res = invoke i8 addrspace(3)* @__mimiccall_int(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12, i256 %mimic)
    to label %ok unwind label %err
ok:
  %res.1u = insertvalue {i8 addrspace(3)*, i1} undef, i8 addrspace(3)* %invoke_res, 0
  %res.1f = insertvalue {i8 addrspace(3)*, i1} %res.1u, i1 1, 1
  ret {i8 addrspace(3)*, i1} %res.1f

err:
  %res.2u = landingpad {i8 addrspace(3)*, i1} cleanup
  %res.2f = insertvalue {i8 addrspace(3)*, i1} %res.2u, i1 0, 1
  ret {i8 addrspace(3)*, i1} %res.2f
}

define {i8 addrspace(3)*, i1} @__farcall_byref(i8 addrspace(3)* %abi_params.r, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12) #3 personality i32 ()* @__personality {
entry:
  %abi_params = ptrtoint i8 addrspace(3)* %abi_params.r to i256
  %invoke_res = invoke i8 addrspace(3)* @__farcall_int(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12)
    to label %ok unwind label %err
ok:
  %res.1u = insertvalue {i8 addrspace(3)*, i1} undef, i8 addrspace(3)* %invoke_res, 0
  %res.1f = insertvalue {i8 addrspace(3)*, i1} %res.1u, i1 1, 1
  ret {i8 addrspace(3)*, i1} %res.1f

err:
  %res.2u = landingpad {i8 addrspace(3)*, i1} cleanup
  %res.2f = insertvalue {i8 addrspace(3)*, i1} %res.2u, i1 0, 1
  ret {i8 addrspace(3)*, i1} %res.2f
}

define {i8 addrspace(3)*, i1} @__staticcall_byref(i8 addrspace(3)* %abi_params.r, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12) #3 personality i32 ()* @__personality {
entry:
  %abi_params = ptrtoint i8 addrspace(3)* %abi_params.r to i256
  %invoke_res = invoke i8 addrspace(3)* @__staticcall_int(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12)
    to label %ok unwind label %err
ok:
  %res.1u = insertvalue {i8 addrspace(3)*, i1} undef, i8 addrspace(3)* %invoke_res, 0
  %res.1f = insertvalue {i8 addrspace(3)*, i1} %res.1u, i1 1, 1
  ret {i8 addrspace(3)*, i1} %res.1f

err:
  %res.2u = landingpad {i8 addrspace(3)*, i1} cleanup
  %res.2f = insertvalue {i8 addrspace(3)*, i1} %res.2u, i1 0, 1
  ret {i8 addrspace(3)*, i1} %res.2f
}

define {i8 addrspace(3)*, i1} @__delegatecall_byref(i8 addrspace(3)* %abi_params.r, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12) #3 personality i32 ()* @__personality {
entry:
  %abi_params = ptrtoint i8 addrspace(3)* %abi_params.r to i256
  %invoke_res = invoke i8 addrspace(3)* @__delegatecall_int(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12)
    to label %ok unwind label %err
ok:
  %res.1u = insertvalue {i8 addrspace(3)*, i1} undef, i8 addrspace(3)* %invoke_res, 0
  %res.1f = insertvalue {i8 addrspace(3)*, i1} %res.1u, i1 1, 1
  ret {i8 addrspace(3)*, i1} %res.1f

err:
  %res.2u = landingpad {i8 addrspace(3)*, i1} cleanup
  %res.2f = insertvalue {i8 addrspace(3)*, i1} %res.2u, i1 0, 1
  ret {i8 addrspace(3)*, i1} %res.2f
}

define {i8 addrspace(3)*, i1} @__mimiccall_byref(i8 addrspace(3)* %abi_params.r, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12, i256 %mimic) #3 personality i32 ()* @__personality {
entry:
  %abi_params = ptrtoint i8 addrspace(3)* %abi_params.r to i256
  %invoke_res = invoke i8 addrspace(3)* @__mimiccall_int(i256 %abi_params, i256 %address, i256 %p3, i256 %p4, i256 %p5, i256 %p6, i256 %p7, i256 %p8, i256 %p9, i256 %p10, i256 %p11, i256 %p12, i256 %mimic)
    to label %ok unwind label %err
ok:
  %res.1u = insertvalue {i8 addrspace(3)*, i1} undef, i8 addrspace(3)* %invoke_res, 0
  %res.1f = insertvalue {i8 addrspace(3)*, i1} %res.1u, i1 1, 1
  ret {i8 addrspace(3)*, i1} %res.1f

err:
  %res.2u = landingpad {i8 addrspace(3)*, i1} cleanup
  %res.2f = insertvalue {i8 addrspace(3)*, i1} %res.2u, i1 0, 1
  ret {i8 addrspace(3)*, i1} %res.2f
}

declare i8 addrspace(3)* @__farcall_int(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256)
declare i8 addrspace(3)* @__staticcall_int(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256)
declare i8 addrspace(3)* @__delegatecall_int(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256)
declare i8 addrspace(3)* @__mimiccall_int(i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256, i256)
declare i32 @__personality()

attributes #0 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
attributes #1 = { mustprogress nofree norecurse nosync nounwind willreturn }
attributes #2 = { nofree norecurse nosync nounwind }
attributes #3 = { noinline nounwind willreturn }
