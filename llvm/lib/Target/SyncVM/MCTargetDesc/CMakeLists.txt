add_llvm_component_library(LLVMSyncVMDesc
  SyncVMAsmBackend.cpp
  SyncVMELFObjectWriter.cpp
  SyncVMELFStreamer.cpp
  SyncVMInstPrinter.cpp
  SyncVMMCAsmInfo.cpp
  SyncVMMCCodeEmitter.cpp
  SyncVMMCTargetDesc.cpp
  SyncVMTargetStreamer.cpp

  ADD_TO_COMPONENT
  SyncVM
  )
