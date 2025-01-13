include_directories(
  ${LLVM_MAIN_SRC_DIR}/lib/Target/EraVM
  )

set(LLVM_LINK_COMPONENTS
  Core
  EraVM
  MC
  MCC
  Object
  Support
  Target
  TargetParser
  )

add_llvm_unittest(EraVMAsmTests
  AssemblerTest.cpp
  )
