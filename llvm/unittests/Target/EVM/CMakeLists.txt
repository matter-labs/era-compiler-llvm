include_directories(
  ${LLVM_MAIN_SRC_DIR}/lib/Target/EVM
  ${LLVM_BINARY_DIR}/lib/Target/EVM
  )

set(LLVM_LINK_COMPONENTS
  EVM
  EVMDesc
  EVMInfo
  CodeGen
  Core
  MC
  MIRParser
  SelectionDAG
  Support
  TargetParser
)

add_llvm_target_unittest(EVMTests
  StackShuffler.cpp
  )

set_property(TARGET EVMTests PROPERTY FOLDER "Tests/UnitTests/TargetTests")
