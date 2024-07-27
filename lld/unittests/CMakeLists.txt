set_target_properties(LLDUnitTests PROPERTIES FOLDER "lld tests")

function(add_lld_unittests test_dirname)
  add_unittest(LLDUnitTests ${test_dirname} ${ARGN})
endfunction()

# Temporary disable on Windows due to non-zero exit code returned by ld.lld
if (NOT WIN32 AND NOT CYGWIN)
  add_subdirectory(AsLibAll)
  add_subdirectory(AsLibELF)
endif()
