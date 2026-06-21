score_use_system(use_sys r8brain)
if(use_sys)
  find_package(r8brain GLOBAL QUIET)
  if(NOT TARGET r8brain)
    find_library(R8BRAIN_LIBRARY NAMES r8brain)
    find_path(R8BRAIN_INCLUDE_DIR CDSPResampler.h PATH_SUFFIXES r8brain)
    if(R8BRAIN_LIBRARY AND R8BRAIN_INCLUDE_DIR)
      add_library(r8brain INTERFACE IMPORTED GLOBAL)
      target_include_directories(r8brain SYSTEM INTERFACE "${R8BRAIN_INCLUDE_DIR}")
      target_link_libraries(r8brain INTERFACE "${R8BRAIN_LIBRARY}")
    endif()
  endif()
endif()

if(NOT TARGET r8brain)
add_library(r8brain STATIC
    "${CMAKE_CURRENT_LIST_DIR}/libossia/3rdparty/r8brain-free-src/r8bbase.cpp"
)

if(NOT MSVC AND NOT CMAKE_CROSSCOMPILING)
  if(CMAKE_BUILD_TYPE MATCHES ".*Debug.*")
    target_compile_options(r8brain PRIVATE -O3 -march=native)
  endif()
endif()

target_include_directories(
  r8brain
  PUBLIC
     "${CMAKE_CURRENT_LIST_DIR}/libossia/3rdparty/r8brain-free-src/"
)
endif()
