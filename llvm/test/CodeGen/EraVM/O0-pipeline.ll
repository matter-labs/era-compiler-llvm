; RUN: llc -O0 -debug-pass=Structure < %s -o /dev/null 2>&1 | FileCheck %s
target triple = "eravm"

; REQUIRES: asserts

; CHECK-LABEL: Pass Arguments:
; CHECK-NEXT: Target Library Information
; CHECK-NEXT: Target Pass Configuration
; CHECK-NEXT: Machine Module Information
; CHECK-NEXT: Target Transform Information
; CHECK-NEXT: Assumption Cache Tracker
; CHECK-NEXT: Create Garbage Collector Module Metadata
; CHECK-NEXT: Profile summary info
; CHECK-NEXT: Machine Branch Probability Analysis
; CHECK-NEXT:   ModulePass Manager
; CHECK-NEXT:     Pre-ISel Intrinsic Lowering
; CHECK-NEXT:     EraVM Lower Intrinsics
; CHECK-NEXT:     FunctionPass Manager
; CHECK-NEXT:       Dominator Tree Construction
; CHECK-NEXT:       Natural Loop Information
; CHECK-NEXT:       Scalar Evolution Analysis
; CHECK-NEXT:       Loop Pass Manager
; CHECK-NEXT:         EraVM Recognize Indexed Load/Store
; CHECK-NEXT:     Link runtime library into the module
; CHECK-NEXT:     FunctionPass Manager
; CHECK-NEXT:       Final transformations before code generation
; CHECK-NEXT:     Dead Global Elimination
; CHECK-NEXT:     FunctionPass Manager
; CHECK-NEXT:       EraVM specific alloca hoisting
; CHECK-NEXT:       Module Verifier
; CHECK-NEXT:       Lower constant intrinsics
; CHECK-NEXT:       Remove unreachable blocks from the CFG
; CHECK-NEXT:       Expand vector predication intrinsics
; CHECK-NEXT:       Scalarize Masked Memory Intrinsics
; CHECK-NEXT:       Expand reduction intrinsics
; CHECK-NEXT:       Prepare callbr
; CHECK-NEXT:       Safe Stack instrumentation pass
; CHECK-NEXT:       Insert stack protectors
; CHECK-NEXT:       Module Verifier
; CHECK-NEXT:       Assignment Tracking Analysis
; CHECK-NEXT:       EraVM DAG->DAG Pattern Instruction Selection
; CHECK-NEXT:       Finalize ISel and expand pseudo-instructions
; CHECK-NEXT:       Local Stack Slot Allocation
; CHECK-NEXT:       EraVM stack address constant propagation
; CHECK-NEXT:       EraVM bytes to cells
; CHECK-NEXT:       Eliminate PHI nodes for register allocation
; CHECK-NEXT:       Two-Address instruction pass
; CHECK-NEXT:       Fast Register Allocator
; CHECK-NEXT:       Remove Redundant DEBUG_VALUE analysis
; CHECK-NEXT:       Fixup Statepoint Caller Saved
; CHECK-NEXT:       Lazy Machine Block Frequency Analysis
; CHECK-NEXT:       Machine Optimization Remark Emitter
; CHECK-NEXT:       Prologue/Epilogue Insertion & Frame Finalization
; CHECK-NEXT:       Post-RA pseudo instruction expansion pass
; CHECK-NEXT:       EraVM expand pseudo instructions
; CHECK-NEXT:       Analyze Machine Code For Garbage Collection
; CHECK-NEXT:       Insert fentry calls
; CHECK-NEXT:       Insert XRay ops
; CHECK-NEXT:       ReachingDefAnalysis
; CHECK-NEXT:       MachineDominator Tree Construction
; CHECK-NEXT:       EraVM combine instuctions to use complex addressing modes
; CHECK-NEXT:       EraVM expand select pseudo instructions
; CHECK-NEXT:       ReachingDefAnalysis
; CHECK-NEXT:       EraVM select optimization postRA
; CHECK-NEXT:       Live DEBUG_VALUE analysis
; CHECK-NEXT:       Machine Sanitizer Binary Metadata
; CHECK-NEXT:       Lazy Machine Block Frequency Analysis
; CHECK-NEXT:       Machine Optimization Remark Emitter
; CHECK-NEXT:       Stack Frame Layout Analysis
; CHECK-NEXT:       EraVM Assembly Printer
; CHECK-NEXT:       Free MachineFunction

define void @f() {
  ret void
}
