set(LLVM_TARGET_DEFINITIONS SyncVM.td)

tablegen(LLVM SyncVMGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM SyncVMGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM SyncVMGenCallingConv.inc -gen-callingconv)
tablegen(LLVM SyncVMGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM SyncVMGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM SyncVMGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM SyncVMGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM SyncVMGenRegisterInfo.inc -gen-register-info)

add_public_tablegen_target(SyncVMCommonTableGen)

add_llvm_target(SyncVMCodeGen
  SyncVMTargetMachine.cpp
  SyncVMRegisterInfo.cpp
  SyncVMAsmPrinter.cpp
  SyncVMInstrInfo.cpp
  SyncVMISelDAGToDAG.cpp
  SyncVMISelLowering.cpp
  SyncVMMachineFunctionInfo.cpp
  SyncVMMCInstLower.cpp
  )

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
