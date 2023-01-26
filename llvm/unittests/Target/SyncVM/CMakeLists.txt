include_directories(
  ${LLVM_MAIN_SRC_DIR}/lib/Target/SyncVM
  ${LLVM_BINARY_DIR}/lib/Target/SyncVM
  )

set(LLVM_LINK_COMPONENTS
  SyncVMCodeGen
  SyncVMDesc
  SyncVMInfo
  CodeGen
  Core
  MC
  MIRParser
  SelectionDAG
  Support
  Target
)

add_llvm_target_unittest(SyncVMTests
  InstrMappingTest.cpp
  )

set_property(TARGET SyncVMTests PROPERTY FOLDER "Tests/UnitTests/TargetTests")
