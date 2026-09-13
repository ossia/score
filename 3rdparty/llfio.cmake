score_use_system(use_sys llfio)
set(LLFIO_INC "${CMAKE_CURRENT_LIST_DIR}/llfio/include")
if(use_sys)
  find_path(LLFIO_INCLUDE_DIR llfio/llfio.hpp)
  if(LLFIO_INCLUDE_DIR)
    set(LLFIO_INC "${LLFIO_INCLUDE_DIR}")
  endif()
endif()

if(NOT TARGET llfio)
  add_library(llfio INTERFACE IMPORTED GLOBAL)
  target_include_directories(llfio SYSTEM INTERFACE "${LLFIO_INC}")
  # ntkernel-error-category lives beside the vendored headers, so what matters is
  # whether those are the ones in use -- not whether system libraries were asked
  # for, since find_path can fail and fall back to them.
  if(WIN32 AND LLFIO_INC STREQUAL "${CMAKE_CURRENT_LIST_DIR}/llfio/include")
    target_include_directories(llfio SYSTEM INTERFACE
      "${CMAKE_CURRENT_LIST_DIR}/llfio/include/llfio/ntkernel-error-category/include")
  endif()
endif()
