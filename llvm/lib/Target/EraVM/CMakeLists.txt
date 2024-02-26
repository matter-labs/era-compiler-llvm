add_llvm_component_group(EraVM)

set(LLVM_TARGET_DEFINITIONS EraVM.td)

tablegen(LLVM EraVMGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM EraVMGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM EraVMGenCallingConv.inc -gen-callingconv)
tablegen(LLVM EraVMGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM EraVMGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM EraVMGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM EraVMGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM EraVMGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM EraVMGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(EraVMCommonTableGen)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/eravm-runtime.ll RT_CONTENT)
set (RT_CONTENT "R\"(${RT_CONTENT})\"")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/EraVMRT.inc ${RT_CONTENT})
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/eravm-stdlib.ll STDLIB_CONTENT)
set (STDLIB_CONTENT "R\"(${STDLIB_CONTENT})\"")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/EraVMStdLib.inc ${STDLIB_CONTENT})

add_llvm_target(EraVMCodeGen
  EraVMAddConditions.cpp
  EraVMAliasAnalysis.cpp
  EraVMAllocaHoisting.cpp
  EraVMAlwaysInline.cpp
  EraVMAsmPrinter.cpp
  EraVMBytesToCells.cpp
  EraVMCodegenPrepare.cpp
  EraVMCombineAddressingMode.cpp
  EraVMCombineFlagSetting.cpp
  EraVMCombineToIndexedMemops.cpp
  EraVMCSE.cpp
  EraVMExpandPseudoInsts.cpp
  EraVMExpandSelect.cpp
  EraVMFoldSimilarInstructions.cpp
  EraVMFrameLowering.cpp
  EraVMHoistFlagSetting.cpp
  EraVMISelDAGToDAG.cpp
  EraVMISelLowering.cpp
  EraVMInstrInfo.cpp
  EraVMLinkRuntime.cpp
  EraVMLowerIntrinsics.cpp
  EraVMMCExpr.cpp
  EraVMMCInstLower.cpp
  EraVMMachineFunctionInfo.cpp
  EraVMMovExpansion.cpp
  EraVMOptimizeSelectPostRA.cpp
  EraVMOptimizeSelectPreRA.cpp
  EraVMOptimizeStdLibCalls.cpp
  EraVMRegisterInfo.cpp
  EraVMSHA3ConstFolding.cpp
  EraVMStackAddressConstantPropagation.cpp
  EraVMSubtarget.cpp
  EraVMTargetMachine.cpp
  EraVMTargetTransformInfo.cpp
  EraVMIndexedMemOpsPrepare.cpp
  EraVMTieSelectOperands.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  Core
  IPO
  MC
  Passes
  Scalar
  SelectionDAG
  Support
  Target
  TransformUtils

  ADD_TO_COMPONENT
  EraVM

  DEPENDS
  intrinsics_gen
  )

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
