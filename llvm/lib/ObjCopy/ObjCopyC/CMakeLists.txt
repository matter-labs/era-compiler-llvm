add_llvm_component_library(LLVMObjCopyC
  ObjCopyC.cpp

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  BinaryFormat
  Core
  MC
  ObjCopy
  Object
  Support
)
