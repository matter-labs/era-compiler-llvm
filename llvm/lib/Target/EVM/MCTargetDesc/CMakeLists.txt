add_llvm_component_library(LLVMEVMDesc
  EVMAsmBackend.cpp
  EVMELFObjectWriter.cpp
  EVMInstPrinter.cpp
  EVMMCAsmInfo.cpp
  EVMMCCodeEmitter.cpp
  EVMMCExpr.cpp
  EVMMCTargetDesc.cpp
  EVMTargetStreamer.cpp

  LINK_COMPONENTS
  MC
  EVMInfo
  Support

  ADD_TO_COMPONENT
  EVM
  )
