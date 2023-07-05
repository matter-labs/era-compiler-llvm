add_llvm_component_library(LLVMEVMInfo
  EVMTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  EVM
  )

add_dependencies(LLVMEVMInfo EVMCommonTableGen)
