add_lld_library(lldC
  LLDAsLibraryC.cpp

  ${LLD_INCLUDE_DIR}/lld/Common

  LINK_COMPONENTS
  Core

  DEPENDS
  intrinsics_gen
  )
