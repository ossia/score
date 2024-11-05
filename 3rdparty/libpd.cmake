if(OSSIA_USE_SYSTEM_LIBRARIES AND LINUX)
  find_library(LIBPD_LIBRARY
    NAMES pd
  )
  find_path(LIBPD_INCLUDE_DIR
    z_libpd.h
    PATH_SUFFIXES pd
  )

  if(LIBPD_LIBRARY AND LIBPD_INCLUDE_DIR)
    add_library(libpd INTERFACE IMPORTED GLOBAL)
    target_link_libraries(libpd INTERFACE "${LIBPD_LIBRARY}")
    target_include_directories(libpd INTERFACE "$<BUILD_INTERFACE:${LIBPD_INCLUDE_DIR}>")
    return()
  endif()
endif()

add_subdirectory("${3RDPARTY_FOLDER}/libpd" "${CMAKE_BINARY_DIR}/libpd")
