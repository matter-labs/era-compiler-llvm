; RUN: opt -mtriple=eravm -O0 -debug-pass-manager < %s -S 2>&1 | FileCheck %s --check-prefix=OPT-O0
; RUN: opt -mtriple=eravm -O1 -debug-pass-manager < %s -S 2>&1 | FileCheck %s --check-prefix=OPT-O1
; RUN: opt -mtriple=eravm -O2 -debug-pass-manager < %s -S 2>&1 | FileCheck %s --check-prefix=OPT-O2
; RUN: opt -mtriple=eravm -O3 -debug-pass-manager < %s -S 2>&1 | FileCheck %s --check-prefix=OPT-O3
; RUN: opt -mtriple=eravm -Os -debug-pass-manager < %s -S 2>&1 | FileCheck %s --check-prefix=OPT-OS
; RUN: opt -mtriple=eravm -Oz -debug-pass-manager < %s -S 2>&1 | FileCheck %s --check-prefix=OPT-OZ

; OPT-O0-NOT: Running pass: MergeSimilarBB on f
; OPT-O1-NOT: Running pass: MergeSimilarBB on f
; OPT-O2: Running pass: MergeSimilarBB on f
; OPT-O2-NEXT: Running pass: SimplifyCFGPass on f
; OPT-O3: Running pass: MergeSimilarBB on f
; OPT-O3-NEXT: Running pass: SimplifyCFGPass on f
; OPT-OS: Running pass: MergeSimilarBB on f
; OPT-OS-NEXT: Running pass: SimplifyCFGPass on f
; OPT-OZ: Running pass: MergeSimilarBB on f
; OPT-OZ-NEXT: Running pass: SimplifyCFGPass on f
define void @f() {
  unreachable
}
