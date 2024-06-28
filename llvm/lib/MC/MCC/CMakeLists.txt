add_llvm_component_library(LLVMMCC
  AssemblerC.cpp

  LINK_COMPONENTS
  AsmParser
  MC
  MCParser
  Support

  DEPENDS
  intrinsics_gen
)
