add_llvm_component_group(EraVM)

set(LLVM_TARGET_DEFINITIONS EraVM.td)

tablegen(LLVM EraVMGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM EraVMGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM EraVMGenMCPseudoLowering.inc -gen-pseudo-lowering)
tablegen(LLVM EraVMGenCallingConv.inc -gen-callingconv)
tablegen(LLVM EraVMGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM EraVMGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM EraVMGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM EraVMGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM EraVMGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM EraVMGenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM EraVMGenSearchableTables.inc -gen-searchable-tables)

add_public_tablegen_target(EraVMCommonTableGen)

set(ERAVM_RUNTIME_PATH_IN ${CMAKE_CURRENT_SOURCE_DIR}/eravm-runtime.ll)
set(ERAVM_RUNTIME_PATH_OUT ${CMAKE_CURRENT_BINARY_DIR}/EraVMRT.inc)
set(ERAVM_STDLIB_PATH_IN ${CMAKE_CURRENT_SOURCE_DIR}/eravm-stdlib.ll)
set(ERAVM_STDLIB_PATH_OUT ${CMAKE_CURRENT_BINARY_DIR}/EraVMStdLib.inc)

file(
  GENERATE
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/EraVMGenRuntime.cmake"
  CONTENT [[
file(READ ${RT_PATH_IN} RT_CONTENT)
set (RT_CONTENT "R\"(${RT_CONTENT})\"")
file(WRITE ${RT_PATH_OUT} ${RT_CONTENT})
  ]]
)

add_custom_command(
  OUTPUT "${ERAVM_RUNTIME_PATH_OUT}"
  COMMAND ${CMAKE_COMMAND} -DRT_PATH_IN=${ERAVM_RUNTIME_PATH_IN}
                           -DRT_PATH_OUT=${ERAVM_RUNTIME_PATH_OUT}
                           -P ${CMAKE_CURRENT_BINARY_DIR}/EraVMGenRuntime.cmake
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/EraVMGenRuntime.cmake
          ${ERAVM_RUNTIME_PATH_IN}
)
add_custom_command(
  OUTPUT "${ERAVM_STDLIB_PATH_OUT}"
  COMMAND ${CMAKE_COMMAND} -DRT_PATH_IN=${ERAVM_STDLIB_PATH_IN}
                           -DRT_PATH_OUT=${ERAVM_STDLIB_PATH_OUT}
                           -P ${CMAKE_CURRENT_BINARY_DIR}/EraVMGenRuntime.cmake
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/EraVMGenRuntime.cmake
          ${ERAVM_STDLIB_PATH_IN}
)
add_custom_target(EraVMRT DEPENDS "${ERAVM_RUNTIME_PATH_OUT}")
add_custom_target(EraVMStdLib DEPENDS "${ERAVM_STDLIB_PATH_OUT}")

add_llvm_target(EraVMCodeGen
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
  EraVMDeadRegisterDefinitions.cpp
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
  EraVMOptimizeSelectPostRA.cpp
  EraVMOptimizeSelectPreRA.cpp
  EraVMOptimizeStdLibCalls.cpp
  EraVMPostCodegenPrepare.cpp
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
  EraVMDesc
  EraVMInfo
  IPO
  IRReader
  Linker
  MC
  Passes
  Scalar
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  EraVM

  DEPENDS
  intrinsics_gen
  EraVMRT
  EraVMStdLib
  )

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
