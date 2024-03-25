add_llvm_component_library(LLVMEraVMDisassembler
  EraVMDisassembler.cpp

  LINK_COMPONENTS
  EraVMDesc
  EraVMInfo
  MCDisassembler
  Support

  ADD_TO_COMPONENT
  EraVM
  )
