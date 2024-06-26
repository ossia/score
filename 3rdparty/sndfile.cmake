if(EMSCRIPTEN)
  return()
endif()

if(SCORE_USE_SYSTEM_LIBRARIES)
  find_package(SndFile GLOBAL CONFIG)
  if(NOT TARGET SndFile::sndfile)
    if(NOT TARGET sndfile)
      find_library(SNDFILE_LIBRARY NAMES SndFile sndfile)
      find_path(SNDFILE_INCLUDE_DIR sndfile.h)
      if(SNDFILE_LIBRARY AND SNDFILE_INCLUDE_DIR)
        add_library(sndfile IMPORTED INTERFACE GLOBAL)
        add_library(SndFile::sndfile ALIAS sndfile)
        target_include_directories(sndfile INTERFACE "${SNDFILE_INCLUDE_DIR}")
        target_link_libraries(sndfile INTERFACE "${SNDFILE_LIBRARY}")
      endif()
   endif()
  endif()
else()
  disable_var(BUILD_PROGRAMS)
  disable_var(BUILD_EXAMPLES)
  disable_var(ENABLE_CPACK)
  disable_var(ENABLE_PACKAGE_CONFIG)
  disable_var(INSTALL_PKGCONFIG_MODULE)
  disable_var(INSTALL_MANPAGES)
  disable_var(ENABLE_BOW_DOCS)
  disable_var(ENABLE_EXPERIMENTAL)
  disable_var(ENABLE_EXTERNAL_LIBS)
  disable_var(ENABLE_MPEG)
  disable_var(BUILD_REGTEST)
  disable_var(BUILD_TESTING)
  disable_var(BUILD_SHARED_LIBS)

  add_subdirectory(3rdparty/libsndfile)

  if(TARGET sndfile)
    set_target_properties(sndfile PROPERTIES UNITY_BUILD 0)
  endif()

  restore_var(BUILD_SHARED_LIBS)
  restore_var(BUILD_TESTING)
endif()
