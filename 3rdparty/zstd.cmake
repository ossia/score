if(SCORE_USE_SYSTEM_LIBRARIES)
  find_package(zstd GLOBAL CONFIG)
endif()

if(NOT TARGET zstd::libzstd_static AND NOT TARGET zstd::libzstd_shared AND NOT TARGET zstd)
  set(ZSTD_BUILD_PROGRAMS OFF CACHE INTERNAL "" FORCE)
  set(ZSTD_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
  set(ZSTD_BUILD_SHARED OFF CACHE INTERNAL "" FORCE)
  set(ZSTD_BUILD_STATIC ON CACHE INTERNAL "" FORCE)
  set(ZSTD_BUILD_DICTBUILDER OFF CACHE INTERNAL "" FORCE)

  set(old_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF)

  if(NOT MSVC AND NOT CMAKE_CROSSCOMPILING)
    if(CMAKE_BUILD_TYPE MATCHES ".*Deb.*")
      set(old_CFLAGS "${CMAKE_C_FLAGS}")
      set(old_CXXFLAGS "${CMAKE_CXX_FLAGS}")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
    endif()
  endif()

  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zstd/build/cmake" EXCLUDE_FROM_ALL)

  if(NOT MSVC AND NOT CMAKE_CROSSCOMPILING)
    if(CMAKE_BUILD_TYPE MATCHES ".*Deb.*")
      set(CMAKE_C_FLAGS "${old_CFLAGS}")
      set(CMAKE_CXX_FLAGS "${old_CXXFLAGS}")
    endif()
  endif()

  set(BUILD_SHARED_LIBS ${old_BUILD_SHARED_LIBS})
endif()