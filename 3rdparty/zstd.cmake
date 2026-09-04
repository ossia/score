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

  # zstd is not unity-safe and must never inherit score's global unity build:
  # lib/legacy/zstd_v0*.c each define their own static FSE_decodeSymbolFast,
  # FSE_endOfDState and FSE_decompress_usingDTable_generic, and
  # dictBuilder/cover.c and fastcover.c both include cover.h. Batched into one
  # translation unit they collide.
  set(old_CMAKE_UNITY_BUILD ${CMAKE_UNITY_BUILD})
  set(CMAKE_UNITY_BUILD OFF)

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
  set(CMAKE_UNITY_BUILD ${old_CMAKE_UNITY_BUILD})
endif()

# Make later find_package(zstd) calls (e.g. 3rdparty/spz) resolve to the
# targets configured above: some prebuilt SDKs ship zstd configs pointing to
# files that do not exist, and a not-found result would trigger FetchContent
# fallbacks that clash with the vendored targets.
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/zstd-config.cmake" [=[
if(TARGET libzstd_static AND NOT TARGET zstd::libzstd_static)
  add_library(zstd::libzstd_static INTERFACE IMPORTED GLOBAL)
  target_link_libraries(zstd::libzstd_static INTERFACE libzstd_static)
endif()
if(TARGET libzstd_shared AND NOT TARGET zstd::libzstd_shared)
  add_library(zstd::libzstd_shared INTERFACE IMPORTED GLOBAL)
  target_link_libraries(zstd::libzstd_shared INTERFACE libzstd_shared)
endif()
]=])