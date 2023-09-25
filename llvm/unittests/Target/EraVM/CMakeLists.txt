include_directories(
  ${LLVM_MAIN_SRC_DIR}/lib/Target/EraVM
  ${LLVM_BINARY_DIR}/lib/Target/EraVM
  )

set(LLVM_LINK_COMPONENTS
  EraVMCodeGen
  EraVMDesc
  EraVMInfo
  CodeGen
  Core
  MC
  MIRParser
  SelectionDAG
  Support
  Target
)

add_llvm_target_unittest(EraVMTests
  InstrMappingTest.cpp
  )

set_property(TARGET EraVMTests PROPERTY FOLDER "Tests/UnitTests/TargetTests")
