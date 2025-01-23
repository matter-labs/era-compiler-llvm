add_lld_unittests(EVMLLDTests
  LLDTest.cpp
)

target_link_libraries(EVMLLDTests
  PRIVATE
  lldCommon
  lldC
  lldELF
  LLVMObjCopy
  LLVMObjCopyC
)
