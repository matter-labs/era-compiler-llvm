set(LLVM_TARGET_DEFINITIONS SyncVM.td)

tablegen(LLVM SyncVMGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM SyncVMGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM SyncVMGenCallingConv.inc -gen-callingconv)
tablegen(LLVM SyncVMGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM SyncVMGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM SyncVMGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM SyncVMGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM SyncVMGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM SyncVMGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(SyncVMCommonTableGen)

add_llvm_target(SyncVMCodeGen
  SyncVMTargetMachine.cpp
  SyncVMSubtarget.cpp
  SyncVMRegisterInfo.cpp
  SyncVMInstrInfo.cpp
  SyncVMMCExpr.cpp
  SyncVMMCInstLower.cpp
  SyncVMAsmPrinter.cpp
  SyncVMMachineFunctionInfo.cpp
  SyncVMFrameLowering.cpp
  SyncVMISelDAGToDAG.cpp
  SyncVMISelLowering.cpp
  SyncVMCodegenPrepare.cpp
  SyncVMExpandPseudoInsts.cpp
  SyncVMAdjustSPBasedOffsets.cpp
  SyncVMLowerIntrinsics.cpp
  SyncVMAllocaHoisting.cpp
  SyncVMTargetTransformInfo.cpp
  SyncVMDropUnusedRegisters.cpp
  SyncVMMoveCallResultSpill.cpp

  DEPENDS
  intrinsics_gen
  )

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
