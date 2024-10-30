; RUN: llc < %s -mtriple=evm | FileCheck %s

define i256 @swap_second_no_junk(i256 %a1, i256 %a2, i256 %a3, i256 %a4) nounwind {
  %x1 = sub i256 %a4, %a1
  ret i256 %x1
}
