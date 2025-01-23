add_lld_unittests(EVMLLDTests
  LLDTest.cpp
)

target_link_libraries(EVMLLDTests
  PRIVATE
  lldCommon
  lldC
  lldELF
  LLVMCore
  LLVMEVMCodeGen
  LLVMEVMInfo
  LLVMEVMDesc
  LLVMIRReader
  LLVMObjCopy
  LLVMObjCopyC
  LLVMTarget
)
