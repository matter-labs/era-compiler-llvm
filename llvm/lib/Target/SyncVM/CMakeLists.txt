add_llvm_component_group(SyncVM)

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


file(READ ${CMAKE_CURRENT_SOURCE_DIR}/syncvm-runtime.ll RT_CONTENT)
set (RT_CONTENT "R\"(${RT_CONTENT})\"")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/SyncVMRT.inc ${RT_CONTENT})

add_llvm_target(SyncVMCodeGen
  SyncVMAddConditions.cpp
  SyncVMBytesToCells.cpp
  SyncVMCodegenPrepare.cpp
  SyncVMExpandUMA.cpp
  SyncVMExpandPseudoInsts.cpp
  SyncVMExpandSelect.cpp
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
  SyncVMIndirectUMA.cpp
  SyncVMIndirectExternalCall.cpp
  SyncVMLinkRuntime.cpp
  SyncVMLowerIntrinsics.cpp
  SyncVMAllocaHoisting.cpp
  SyncVMTargetTransformInfo.cpp
  SyncVMPeephole.cpp
  SyncVMPropagateGenericPointers.cpp
  SyncVMStackAddressConstantPropagation.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  Core
  IPO
  MC
  Scalar
  SelectionDAG
  Support
  Target
  TransformUtils

  ADD_TO_COMPONENT
  SyncVM

  DEPENDS
  intrinsics_gen
  )

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
