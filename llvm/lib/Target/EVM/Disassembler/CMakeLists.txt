add_llvm_component_library(LLVMEVMDisassembler
  EVMDisassembler.cpp

  LINK_COMPONENTS
  EVMDesc
  MCDisassembler
  EVMInfo
  Support
  MC

  ADD_TO_COMPONENT
  EVM
  )
