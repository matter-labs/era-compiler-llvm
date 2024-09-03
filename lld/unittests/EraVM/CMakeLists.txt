add_lld_unittests(EraVMLLDTests
  LLDTest.cpp
)

target_link_libraries(EraVMLLDTests
  PRIVATE
  lldCommon
  lldC
  lldELF
  LLVMObjCopy
  LLVMObjCopyC
)
