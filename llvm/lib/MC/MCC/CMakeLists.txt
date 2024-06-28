add_llvm_component_library(LLVMMCC
  AssemblerC.cpp

  LINK_COMPONENTS
  AsmParser
  Core
  MC
  MCParser
  Object
  Support

  DEPENDS
  intrinsics_gen
)
