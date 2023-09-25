add_llvm_component_library(LLVMEraVMDesc
  EraVMAsmBackend.cpp
  EraVMELFObjectWriter.cpp
  EraVMELFStreamer.cpp
  EraVMInstPrinter.cpp
  EraVMMCAsmInfo.cpp
  EraVMMCCodeEmitter.cpp
  EraVMMCTargetDesc.cpp
  EraVMTargetStreamer.cpp

  ADD_TO_COMPONENT
  EraVM
  )
