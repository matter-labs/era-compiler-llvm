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

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/evm-stdlib.ll STDLIB_CONTENT)
set (STDLIB_CONTENT "R\"(${STDLIB_CONTENT})\"")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/EVMStdLib.inc ${STDLIB_CONTENT})

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
  EVMTargetObjectFile.cpp
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
  intrinsics_gen
  )

add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
