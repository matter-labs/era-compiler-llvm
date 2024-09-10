; RUN: llc -O3 -debug-pass=Structure < %s -o /dev/null 2>&1 | FileCheck %s
target triple = "eravm"

; REQUIRES: asserts

; CHECK-LABEL: Pass Arguments:
; CHECK-NEXT: Target Library Information
; CHECK-NEXT: Target Pass Configuration
; CHECK-NEXT: Machine Module Information
; CHECK-NEXT: Target Transform Information
; CHECK-NEXT: Assumption Cache Tracker
; CHECK-NEXT: EraVM Address space based Alias Analysis
; CHECK-NEXT: External Alias Analysis
; CHECK-NEXT: Type-Based Alias Analysis
; CHECK-NEXT: Scoped NoAlias Alias Analysis
; CHECK-NEXT: Profile summary info
; CHECK-NEXT: Create Garbage Collector Module Metadata
; CHECK-NEXT: Machine Branch Probability Analysis
; CHECK-NEXT: Default Regalloc Eviction Advisor
; CHECK-NEXT: Default Regalloc Priority Advisor
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
; CHECK-NEXT:       Dominator Tree Construction
; CHECK-NEXT:       Natural Loop Information
; CHECK-NEXT:       Split GEPs to a variadic base and a constant offset for better CSE
; CHECK-NEXT:       Scalar Evolution Analysis
; CHECK-NEXT:       Straight line strength reduction
; CHECK-NEXT:       Basic Alias Analysis (stateless AA impl)
; CHECK-NEXT:       Function Alias Analysis Results
; CHECK-NEXT:       Memory SSA
; CHECK-NEXT:       Global Value Numbering
; CHECK-NEXT:       Post-Dominator Tree Construction
; CHECK-NEXT:       Basic Alias Analysis (stateless AA impl)
; CHECK-NEXT:       Function Alias Analysis Results
; CHECK-NEXT:       Memory Dependence Analysis
; CHECK-NEXT:       Memory SSA
; CHECK-NEXT:       Early GVN Hoisting of Expressions
; CHECK-NEXT:       Natural Loop Information
; CHECK-NEXT:       Scalar Evolution Analysis
; CHECK-NEXT:       Nary reassociation
; CHECK-NEXT:       Early CSE
; CHECK-NEXT:       Basic Alias Analysis (stateless AA impl)
; CHECK-NEXT:       Function Alias Analysis Results
; CHECK-NEXT:       Memory SSA
; CHECK-NEXT:       Canonicalize natural loops
; CHECK-NEXT:       LCSSA Verifier
; CHECK-NEXT:       Loop-Closed SSA Form Pass
; CHECK-NEXT:       Scalar Evolution Analysis
; CHECK-NEXT:       Lazy Branch Probability Analysis
; CHECK-NEXT:       Lazy Block Frequency Analysis
; CHECK-NEXT:       Loop Pass Manager
; CHECK-NEXT:         Loop Invariant Code Motion
; CHECK-NEXT:       Module Verifier
; CHECK-NEXT:       Loop Pass Manager
; CHECK-NEXT:         Canonicalize Freeze Instructions in Loops
; CHECK-NEXT:         Induction Variable Users
; CHECK-NEXT:         Loop Strength Reduction
; CHECK-NEXT:       Basic Alias Analysis (stateless AA impl)
; CHECK-NEXT:       Function Alias Analysis Results
; CHECK-NEXT:       Merge contiguous icmps into a memcmp
; CHECK-NEXT:       Natural Loop Information
; CHECK-NEXT:       Lazy Branch Probability Analysis
; CHECK-NEXT:       Lazy Block Frequency Analysis
; CHECK-NEXT:       Expand memcmp() to load/stores
; CHECK-NEXT:       Lower constant intrinsics
; CHECK-NEXT:       Remove unreachable blocks from the CFG
; CHECK-NEXT:       Natural Loop Information
; CHECK-NEXT:       Post-Dominator Tree Construction
; CHECK-NEXT:       Branch Probability Analysis
; CHECK-NEXT:       Block Frequency Analysis
; CHECK-NEXT:       Constant Hoisting
; CHECK-NEXT:       Replace intrinsics with calls to vector library
; CHECK-NEXT:       Partially inline calls to library functions
; CHECK-NEXT:       Expand vector predication intrinsics
; CHECK-NEXT:       Scalarize Masked Memory Intrinsics
; CHECK-NEXT:       Expand reduction intrinsics
; CHECK-NEXT:       Natural Loop Information
; CHECK-NEXT:       TLS Variable Hoist
; CHECK-NEXT:       CodeGen Prepare
; CHECK-NEXT:       Dominator Tree Construction
; CHECK-NEXT:       Natural Loop Information
; CHECK-NEXT:       EraVM optimizations after CodeGenPrepare pass
; CHECK-NEXT:       Prepare callbr
; CHECK-NEXT:       Safe Stack instrumentation pass
; CHECK-NEXT:       Insert stack protectors
; CHECK-NEXT:       Module Verifier
; CHECK-NEXT:       Dominator Tree Construction
; CHECK-NEXT:       Basic Alias Analysis (stateless AA impl)
; CHECK-NEXT:       Function Alias Analysis Results
; CHECK-NEXT:       Natural Loop Information
; CHECK-NEXT:       Post-Dominator Tree Construction
; CHECK-NEXT:       Branch Probability Analysis
; CHECK-NEXT:       Assignment Tracking Analysis
; CHECK-NEXT:       Lazy Branch Probability Analysis
; CHECK-NEXT:       Lazy Block Frequency Analysis
; CHECK-NEXT:       EraVM DAG->DAG Pattern Instruction Selection
; CHECK-NEXT:       Finalize ISel and expand pseudo-instructions
; CHECK-NEXT:       Lazy Machine Block Frequency Analysis
; CHECK-NEXT:       Early Tail Duplication
; CHECK-NEXT:       Optimize machine instruction PHIs
; CHECK-NEXT:       Slot index numbering
; CHECK-NEXT:       Merge disjoint stack slots
; CHECK-NEXT:       Local Stack Slot Allocation
; CHECK-NEXT:       Remove dead machine instructions
; CHECK-NEXT:       MachineDominator Tree Construction
; CHECK-NEXT:       EraVM condition optimizer
; CHECK-NEXT:       Machine Natural Loop Construction
; CHECK-NEXT:       Machine Block Frequency Analysis
; CHECK-NEXT:       Early Machine Loop Invariant Code Motion
; CHECK-NEXT:       MachineDominator Tree Construction
; CHECK-NEXT:       Machine Block Frequency Analysis
; CHECK-NEXT:       Machine Common Subexpression Elimination
; CHECK-NEXT:       MachinePostDominator Tree Construction
; CHECK-NEXT:       Machine Cycle Info Analysis
; CHECK-NEXT:       Machine code sinking
; CHECK-NEXT:       Peephole Optimizations
; CHECK-NEXT:       Remove dead machine instructions
; CHECK-NEXT:       EraVM stack address constant propagation
; CHECK-NEXT:       EraVM bytes to cells
; CHECK-NEXT:       EraVM combine flag setting
; CHECK-NEXT:       MachineDominator Tree Construction
; CHECK-NEXT:       EraVM combine instructions to generate indexed memory operations
; CHECK-NEXT:       EraVM fold similar instructions
; CHECK-NEXT:       EraVM select optimization preRA
; CHECK-NEXT:       MachineDominator Tree Construction
; CHECK-NEXT:       Machine Natural Loop Construction
; CHECK-NEXT:       EraVM hoist flag setting instruction
; CHECK-NEXT:       Remove unreachable machine basic blocks
; CHECK-NEXT:       Live Variable Analysis
; CHECK-NEXT:       EraVM tie select operands pass
; CHECK-NEXT:       EraVM dead register definitions
; CHECK-NEXT:       Detect Dead Lanes
; CHECK-NEXT:       Process Implicit Definitions
; CHECK-NEXT:       Remove unreachable machine basic blocks
; CHECK-NEXT:       Live Variable Analysis
; CHECK-NEXT:       MachineDominator Tree Construction
; CHECK-NEXT:       Machine Natural Loop Construction
; CHECK-NEXT:       Eliminate PHI nodes for register allocation
; CHECK-NEXT:       Two-Address instruction pass
; CHECK-NEXT:       Slot index numbering
; CHECK-NEXT:       Live Interval Analysis
; CHECK-NEXT:       Register Coalescer
; CHECK-NEXT:       Rename Disconnected Subregister Components
; CHECK-NEXT:       Machine Instruction Scheduler
; CHECK-NEXT:       Machine Block Frequency Analysis
; CHECK-NEXT:       Debug Variable Analysis
; CHECK-NEXT:       Live Stack Slot Analysis
; CHECK-NEXT:       Virtual Register Map
; CHECK-NEXT:       Live Register Matrix
; CHECK-NEXT:       Bundle Machine CFG Edges
; CHECK-NEXT:       Spill Code Placement Analysis
; CHECK-NEXT:       Lazy Machine Block Frequency Analysis
; CHECK-NEXT:       Machine Optimization Remark Emitter
; CHECK-NEXT:       Greedy Register Allocator
; CHECK-NEXT:       Virtual Register Rewriter
; CHECK-NEXT:       Register Allocation Pass Scoring
; CHECK-NEXT:       Stack Slot Coloring
; CHECK-NEXT:       Machine Copy Propagation Pass
; CHECK-NEXT:       Machine Loop Invariant Code Motion
; CHECK-NEXT:       Remove Redundant DEBUG_VALUE analysis
; CHECK-NEXT:       Fixup Statepoint Caller Saved
; CHECK-NEXT:       PostRA Machine Sink
; CHECK-NEXT:       Machine Block Frequency Analysis
; CHECK-NEXT:       MachineDominator Tree Construction
; CHECK-NEXT:       MachinePostDominator Tree Construction
; CHECK-NEXT:       Lazy Machine Block Frequency Analysis
; CHECK-NEXT:       Machine Optimization Remark Emitter
; CHECK-NEXT:       Shrink Wrapping analysis
; CHECK-NEXT:       Prologue/Epilogue Insertion & Frame Finalization
; CHECK-NEXT:       Machine Late Instructions Cleanup Pass
; CHECK-NEXT:       Control Flow Optimizer
; CHECK-NEXT:       Lazy Machine Block Frequency Analysis
; CHECK-NEXT:       Tail Duplication
; CHECK-NEXT:       Machine Copy Propagation Pass
; CHECK-NEXT:       Post-RA pseudo instruction expansion pass
; CHECK-NEXT:       EraVM expand pseudo instructions
; CHECK-NEXT:       MachineDominator Tree Construction
; CHECK-NEXT:       Machine Natural Loop Construction
; CHECK-NEXT:       Machine Block Frequency Analysis
; CHECK-NEXT:       If Converter
; CHECK-NEXT:       MachineDominator Tree Construction
; CHECK-NEXT:       Machine Natural Loop Construction
; CHECK-NEXT:       Post RA top-down list latency scheduler
; CHECK-NEXT:       Analyze Machine Code For Garbage Collection
; CHECK-NEXT:       Machine Block Frequency Analysis
; CHECK-NEXT:       MachinePostDominator Tree Construction
; CHECK-NEXT:       Branch Probability Basic Block Placement
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
; CHECK-NEXT:     Machine Outliner
; CHECK-NEXT:     FunctionPass Manager
; CHECK-NEXT:       Lazy Machine Block Frequency Analysis
; CHECK-NEXT:       Machine Optimization Remark Emitter
; CHECK-NEXT:       Stack Frame Layout Analysis
; CHECK-NEXT:       EraVM Assembly Printer
; CHECK-NEXT:       Free MachineFunction

define void @f() {
  ret void
}
