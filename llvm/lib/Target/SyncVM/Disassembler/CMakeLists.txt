add_llvm_component_library(LLVMSyncVMDisassembler
  SyncVMDisassembler.cpp

  LINK_COMPONENTS
    MC
    MCDisassembler
    Support

  ADD_TO_COMPONENT
  SyncVM
)
