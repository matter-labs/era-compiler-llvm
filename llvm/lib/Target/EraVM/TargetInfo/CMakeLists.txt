add_llvm_component_library(LLVMEraVMInfo
  EraVMTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  EraVM
  )
