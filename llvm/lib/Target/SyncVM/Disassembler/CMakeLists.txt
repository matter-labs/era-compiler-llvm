add_llvm_component_library(LLVMSyncVMDisassembler
  SyncVMDisassembler.cpp

  LINK_COMPONENTS
  MCDisassembler
  SyncVMInfo
  Support

  ADD_TO_COMPONENT
  SyncVM
  )
