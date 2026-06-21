if(SCORE_DISABLE_SNMALLOC)
  return()
endif()

if(EMSCRIPTEN)
  return()
endif()

if(SCORE_HAS_SANITIZERS)
  return()
endif()

if("${CMAKE_CXX_FLAGS}" MATCHES ".*_GLIBCXX_ASSERTIONS.*")
  return()
endif()

score_use_system(use_sys snmalloc)
if(use_sys)
  find_package(snmalloc GLOBAL CONFIG)
else()
  block()
  set(SNMALLOC_BUILD_TESTING
      OFF
      CACHE INTERNAL "" FORCE)
  add_subdirectory(3rdparty/snmalloc EXCLUDE_FROM_ALL SYSTEM)
  endblock()
endif()
