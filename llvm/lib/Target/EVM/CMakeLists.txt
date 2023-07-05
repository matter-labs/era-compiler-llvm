add_llvm_component_group(EVM)

set(LLVM_TARGET_DEFINITIONS EVM.td)

tablegen(LLVM EVMGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM EVMGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM EVMGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM EVMGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM EVMGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM EVMGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM EVMGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(EVMCommonTableGen)

set(EVM_STDLIB_PATH_IN ${CMAKE_CURRENT_SOURCE_DIR}/evm-stdlib.ll)
set(EVM_STDLIB_PATH_OUT ${CMAKE_CURRENT_BINARY_DIR}/EVMStdLib.inc)

file(
  GENERATE
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/EVMGenRuntime.cmake"
  CONTENT [[
file(READ ${RT_PATH_IN} RT_CONTENT)
set (RT_CONTENT "R\"(${RT_CONTENT})\"")
file(WRITE ${RT_PATH_OUT} ${RT_CONTENT})
  ]]
)

add_custom_command(
  OUTPUT "${EVM_STDLIB_PATH_OUT}"
  COMMAND ${CMAKE_COMMAND} -DRT_PATH_IN=${EVM_STDLIB_PATH_IN}
                           -DRT_PATH_OUT=${EVM_STDLIB_PATH_OUT}
                           -P ${CMAKE_CURRENT_BINARY_DIR}/EVMGenRuntime.cmake
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/EVMGenRuntime.cmake
          ${EVM_STDLIB_PATH_IN}
)
add_custom_target(EVMStdLib DEPENDS "${EVM_STDLIB_PATH_OUT}")

add_llvm_target(EVMCodeGen
  EVMAllocaHoisting.cpp
  EVMArgumentMove.cpp
  EVMAsmPrinter.cpp
  EVMCodegenPrepare.cpp
  EVMFrameLowering.cpp
  EVMISelDAGToDAG.cpp
  EVMISelLowering.cpp
  EVMInstrInfo.cpp
  EVMLinkRuntime.cpp
  EVMLowerIntrinsics.cpp
  EVMMachineFunctionInfo.cpp
  EVMMCInstLower.cpp
  EVMOptimizeLiveIntervals.cpp
  EVMRegColoring.cpp
  EVMRegisterInfo.cpp
  EVMSingleUseExpression.cpp
  EVMStackify.cpp
  EVMSubtarget.cpp
  EVMTargetMachine.cpp
  EVMTargetTransformInfo.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  Core
  EVMDesc
  EVMInfo
  IPO
  IRReader
  Linker
  MC
  Scalar
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils
  EVMDesc
  EVMInfo

  ADD_TO_COMPONENT
  EVM

  DEPENDS
  EVMStdLib
  intrinsics_gen
)

add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
