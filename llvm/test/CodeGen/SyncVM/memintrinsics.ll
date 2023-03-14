; RUN: llc -O3 < %s | FileCheck %s
target datalayout = "E-p:256:256-i256:256:256-S32-a:256:256"
target triple = "syncvm"

declare void @llvm.memcpy.p0i256.p0i256.i256(i256 addrspace(0)* noalias nocapture writeonly, i256 addrspace(0)* noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p1i256.p1i256.i256(i256 addrspace(1)* noalias nocapture writeonly, i256 addrspace(1)* noalias nocapture readonly, i256, i1 immarg)
declare void @llvm.memcpy.p2i256.p2i256.i256(i256 addrspace(2)* noalias nocapture writeonly, i256 addrspace(2)* noalias nocapture readonly, i256, i1 immarg)


; CHECK-LABEL: huge-copysize0
define fastcc void @huge-copysize0(i256 addrspace(0)* %dest, i256 addrspace(0)* %src) {
; CHECK:   add     r0, r0, [[INDEX0:r[0-9]+]]
; CHECK: .BB0_1:
; CHECK:   shl.s   5, [[INDEX0]], [[SHIFTED_OFFSET0_SRC:r[0-9]+]]
; CHECK:   add     r1, [[SHIFTED_OFFSET0_SRC]], [[SHIFTED_OFFSET0_DST:r[0-9]+]]
; CHECK:   add     r2, [[SHIFTED_OFFSET0_SRC]], [[SHIFTED_OFFSET0_SRC]]
; CHECK:   div.s   32, [[SHIFTED_OFFSET0_SRC]], [[SHIFTED_OFFSET0_SRC]], r0
; CHECK:   add     stack[[[SHIFTED_OFFSET0_SRC]] - 0], r0, [[LOADED_VALUE0:r[0-9]+]]
; CHECK:   div.s   32, [[SHIFTED_OFFSET0_DST]], [[SHIFTED_OFFSET0_DST]], r0
; CHECK:   add     [[LOADED_VALUE0]], r0, stack[[[SHIFTED_OFFSET0_DST]] - 0]
; CHECK:   add     1, [[INDEX0]], [[INDEX0]]
; CHECK:   sub.s!  @CPI0_0[0], [[INDEX0]], r4
; CHECK:   jump.lt @.BB0_1
; CHECK:   ret
  call void @llvm.memcpy.p0i256.p0i256.i256(i256 addrspace(0)* %dest, i256 addrspace(0)* %src, i256 81129638414606681695789005144064, i1 false)
  ret void
}

; CHECK-LABEL: huge-copysize1
define fastcc void @huge-copysize1(i256 addrspace(1)* %dest, i256 addrspace(1)* %src) {
; CHECK:  add     r0, r0, [[INDEX1:r[0-9]+]]
; CHECK:.BB1_1:
; CHECK:  shl.s   5, [[INDEX1]], [[SHIFTED_OFFSET1_SRC:r[0-9]+]]
; CHECK:  add     r1, [[SHIFTED_OFFSET1_SRC]], [[SHIFTED_OFFSET1_DST:r[0-9]+]]
; CHECK:  add     r2, [[SHIFTED_OFFSET1_SRC]], [[SHIFTED_OFFSET1_SRC]]
; CHECK:  ld.1    [[SHIFTED_OFFSET1_SRC]], [[LOADED_VALUE1:r[0-9]+]]
; CHECK:  st.1    [[SHIFTED_OFFSET1_DST]], [[LOADED_VALUE1]]
; CHECK:  add     1, [[INDEX1]], [[INDEX1]]
; CHECK:  sub.s!  @CPI1_0[0], [[INDEX1]], r{{[0-9]+}}
; CHECK:  jump.lt @.BB1_1
  
; trailing part:
; CHECK:  add     @CPI1_1[0], r1, r1
; CHECK:  ld.1    r1, [[TRAILING_PART1:r[0-9]+]]
; CHECK:  and     @CPI1_2[0], [[TRAILING_DST1:r[0-9]+]], [[TRAILING_DST1]]
; CHECK:  add     @CPI1_1[0], r2, r2
; CHECK:  ld.1    r2, [[TRAILING_SRC1:r[0-9]+]]
; CHECK:  and     @CPI1_3[0], [[TRAILING_SRC1]], [[TRAILING_SRC1]]
; CHECK:  or      [[TRAILING_SRC1]], [[TRAILING_DST1]], [[MERGED1:r[0-9]+]]
; CHECK:  st.1    r1, [[MERGED1:r[0-9]+]]
; CHECK:  ret

  ; the test explicitly has some trailing part to be copied.
  call void @llvm.memcpy.p1i256.p1i256.i256(i256 addrspace(1)* %dest, i256 addrspace(1)* %src, i256 81129638414606681695789005144065, i1 false)
  ret void
}

; CHECK-LABEL: huge-copysize2
define fastcc void @huge-copysize2(i256 addrspace(2)* %dest, i256 addrspace(2)* %src) {
; CHECK:  add     r0, r0, [[INDEX2:r[0-9]+]]
; CHECK:.BB2_1:
; CHECK:  shl.s   5, [[INDEX2]], [[SHIFTED_OFFSET2_SRC:r[0-9]+]]
; CHECK:  add     r1, [[SHIFTED_OFFSET2_SRC]], [[SHIFTED_OFFSET2_DST:r[0-9]+]]
; CHECK:  add     r2, [[SHIFTED_OFFSET2_SRC]], [[SHIFTED_OFFSET2_SRC]]
; CHECK:  ld.2    [[SHIFTED_OFFSET2_SRC]], [[LOADED_VALUE2:r[0-9]+]]
; CHECK:  st.2    [[SHIFTED_OFFSET2_DST]], [[LOADED_VALUE2]]
; CHECK:  add     1, [[INDEX2]], [[INDEX2]]
; CHECK:  sub.s!  @CPI2_0[0], [[INDEX2]], r{{[0-9]+}}
; CHECK:  jump.lt @.BB2_1

; trailing part:
; CHECK:  add     @CPI2_1[0], r1, r1
; CHECK:  ld.2    r1, [[TRAILING_PART2:r[0-9]+]]
; CHECK:  and     @CPI2_2[0], [[TRAILING_DST2:r[0-9]+]], [[TRAILING_DST2]]
; CHECK:  add     @CPI2_1[0], r2, r2
; CHECK:  ld.2    r2, [[TRAILING_SRC2:r[0-9]+]]
; CHECK:  and     @CPI2_3[0], [[TRAILING_SRC2]], [[TRAILING_SRC2]]
; CHECK:  or      [[TRAILING_SRC2]], [[TRAILING_DST2]], [[MERGED2:r[0-9]+]]
; CHECK:  st.2    r1, [[MERGED2:r[0-9]+]]
; CHECK:  ret

  ; the test explicitly has some trailing part to be copied.
  call void @llvm.memcpy.p2i256.p2i256.i256(i256 addrspace(2)* %dest, i256 addrspace(2)* %src, i256 81129638414606681695789005144065, i1 false)
  ret void
}

; CHECK-LABEL: normal-known-size
define fastcc void @normal-known-size(i256* %dest, i256* %src) {
; CHECK:   add     r0, r0, [[INDEX3:r[3-9]+]]
; CHECK: .BB3_1:
; CHECK:   shl.s   5, [[INDEX3]], [[SHIFTED_OFFSET3_SRC:r[3-9]+]]
; CHECK:   add     r1, [[SHIFTED_OFFSET3_SRC]], [[SHIFTED_OFFSET3_DST:r[3-9]+]]
; CHECK:   add     r2, [[SHIFTED_OFFSET3_SRC]], [[SHIFTED_OFFSET3_SRC]]
; CHECK:   div.s   32, [[SHIFTED_OFFSET3_SRC]], [[SHIFTED_OFFSET3_SRC]], r0
; CHECK:   add     stack[[[SHIFTED_OFFSET3_SRC]] - 0], r0, [[LOADED_VALUE3:r[3-9]+]]
; CHECK:   div.s   32, [[SHIFTED_OFFSET3_DST]], [[SHIFTED_OFFSET3_DST]], r0
; CHECK:   add     [[LOADED_VALUE3]], r0, stack[[[SHIFTED_OFFSET3_DST]] - 0]
; CHECK:   add     1, [[INDEX3]], [[INDEX3]]
; CHECK:   sub.s!  32, [[INDEX3]], r{{[0-9]+}}
; CHECK:   jump.lt @.BB3_1
; CHECK:   ret
  call void @llvm.memcpy.p0i256.p0i256.i256(i256* %dest, i256* %src, i256 1024, i1 false)
  ret void
}

; CHECK-LABEL: normal-known-size-2
define fastcc void @normal-known-size-2(i256* %dest, i256* %src) {
; CHECK:   add     r0, r0, [[INDEX4:r[3-9]+]]
; CHECK: .BB4_1:
; CHECK:   shl.s   5, [[INDEX4]], [[SHIFTED_OFFSET4_SRC:r[3-9]+]]
; CHECK:   add     r1, [[SHIFTED_OFFSET4_SRC]], [[SHIFTED_OFFSET4_DST:r[3-9]+]]
; CHECK:   add     r2, [[SHIFTED_OFFSET4_SRC]], [[SHIFTED_OFFSET4_SRC]]
; CHECK:   div.s   32, [[SHIFTED_OFFSET4_SRC]], [[SHIFTED_OFFSET4_SRC]], r0
; CHECK:   add     stack[[[SHIFTED_OFFSET4_SRC]] - 0], r0, [[LOADED_VALUE4:r[3-9]+]]
; CHECK:   div.s   32, [[SHIFTED_OFFSET4_DST]], [[SHIFTED_OFFSET4_DST]], r0
; CHECK:   add     [[LOADED_VALUE4]], r0, stack[[[SHIFTED_OFFSET4_DST]] - 0]
; CHECK:   add     1, [[INDEX4]], [[INDEX4]]
; CHECK:   sub.s!  33, [[INDEX4]], r{{[0-9]+}}
; CHECK:   jump.lt @.BB4_1
; CHECK:   add     @CPI4_0[0], r0, [[SRCMASK4:r[0-9]+]]
; CHECK:   div.s   32, r2, r2, r0
; CHECK:   and     stack[r2 + 33], [[SRCMASK4]], [[SRCMASKED_VALUE4:r[0-9]+]]
; CHECK:   add     @CPI4_1[0], r0, [[DSTMASK4:r[0-9]+]]
; CHECK:   div.s   32, r1, r1, r0
; CHECK:   and     stack[r1 + 33], [[DSTMASK4]], [[DSTMASKED_VALUE4:r[0-9]+]]
; CHECK:   or      [[SRCMASKED_VALUE4]], [[DSTMASKED_VALUE4]], stack[r1 + 33]
  call void @llvm.memcpy.p0i256.p0i256.i256(i256* %dest, i256* %src, i256 1060, i1 false)
  ret void
}

; check that the big size copy has correct number of iterations (size / 32)
; CHECK: CPI0_0:
; CHECK: CPI1_0:
; CHECK: CPI2_0:
; CHECK:        .cell 2535301200456458802993406410752


; check that in the trailing part, the mask is correct
; CHECK: CPI1_1:
; CHECK: CPI2_1:
; CHECK:  .cell 452312848583266388373324160190187140051835877600158453279131187530910662655
; CHECK: CPI1_3:
; CHECK: CPI2_3:
; CHECK:  .cell -452312848583266388373324160190187140051835877600158453279131187530910662656
