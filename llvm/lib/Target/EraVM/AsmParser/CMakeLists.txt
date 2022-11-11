add_llvm_component_library(LLVMEraVMAsmParser
  EraVMAsmParser.cpp

  LINK_COMPONENTS
  CodeGenTypes
  MC
  MCParser
  EraVMDesc
  EraVMInfo
  Support

  ADD_TO_COMPONENT
  EraVM
)
