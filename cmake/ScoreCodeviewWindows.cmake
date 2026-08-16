if(WIN32)
  # Clang only. GCC's CodeView writer segfaults on any boost::container type,
  # which this codebase uses in around thirty translation units, so -gcodeview
  # makes a gcc Debug or RelWithDebInfo build impossible to complete:
  #
  #   $ cat REPRO.cpp
  #   #include <boost/container/vector.hpp>
  #   void f() { boost::container::vector<int> v; (void)v; }
  #   $ g++ -gcodeview -c REPRO.cpp
  #   REPRO.cpp:2:54: internal compiler error: Segmentation fault
  #
  # Reproduced on gcc 16.2.0 (MSYS2 UCRT64) with boost 1.91. The flag alone is
  # enough -- no optimisation level or standard setting is involved -- and DWARF
  # is unaffected, as is clang. The crash is in the type-record emission reached
  # from dwarf2out_finish, which is why every report points at the last line of
  # the file.
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    if(NOT (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
      set(SCORE_COMPILER_NEEDS_GCODEVIEW 1)
    endif()
  endif()

  if (SCORE_COMPILER_NEEDS_GCODEVIEW)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -gcodeview")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -gcodeview")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -gcodeview")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -gcodeview")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -Wl,--pdb=")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} -Wl,--pdb=")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -Wl,--pdb=")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -Wl,--pdb=")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} -Wl,--pdb=")
    set(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO} -Wl,--pdb=")
  endif()
endif()
