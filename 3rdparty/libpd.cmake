if(OSSIA_USE_SYSTEM_LIBRARIES AND LINUX)
  find_library(LIBPD_LIBRARY
    NAMES pd
  )
  find_path(LIBPD_INCLUDE_DIR
    z_libpd.h
    PATH_SUFFIXES pd
  )

  if(LIBPD_LIBRARY AND LIBPD_INCLUDE_DIR)
    include(CheckCXXSourceCompiles)

    set(CMAKE_REQUIRED_LIBRARIES "${LIBPD_LIBRARY}")
    check_cxx_source_compiles("
#if defined(_WIN32)
extern \"C\" { __declspec(dllimport) extern void* pd_getinstance(void); }
int main() {
    void* ptr = (void*)&pd_getinstance;
    return (ptr != 0) ? 0 : 1;
}
#else
extern \"C\" { extern void* pd_this; }
int main() {
    void* ptr = (void*)&pd_this;
    return (ptr != 0) ? 0 : 1;
}
#endif
    " LIBPD_LIBRARY_IS_MULTI)

    if(LIBPD_LIBRARY_IS_MULTI)
      add_library(libpd INTERFACE IMPORTED GLOBAL)
      target_link_libraries(libpd INTERFACE "${LIBPD_LIBRARY}")
      target_include_directories(libpd INTERFACE "$<BUILD_INTERFACE:${LIBPD_INCLUDE_DIR}>")
      return()
    endif()
  endif()
endif()

add_subdirectory("${3RDPARTY_FOLDER}/libpd" "${CMAKE_BINARY_DIR}/libpd")
