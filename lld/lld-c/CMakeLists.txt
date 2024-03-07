add_lld_library(lldC
  LLDAsLibraryC.cpp

  ${LLD_INCLUDE_DIR}/lld/Common

  LINK_COMPONENTS
  Core
  Support

  LINK_LIBS
  lldCommon
  lldELF

  DEPENDS
  intrinsics_gen
  )
