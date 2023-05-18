add_llvm_component_group(EVM)

set(LLVM_TARGET_DEFINITIONS EVM.td)

tablegen(LLVM EVMGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM EVMGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM EVMGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM EVMGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM EVMGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM EVMGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(EVMCommonTableGen)

add_llvm_target(EVMCodeGen
  EVMAsmPrinter.cpp
  EVMFrameLowering.cpp
  EVMISelDAGToDAG.cpp
  EVMISelLowering.cpp
  EVMInstrInfo.cpp
  EVMMCInstLower.cpp
  EVMRegisterInfo.cpp
  EVMSubtarget.cpp
  EVMTargetMachine.cpp

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
  EVM

  DEPENDS
  intrinsics_gen
  )

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
