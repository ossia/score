add_library(r8brain STATIC
    "${CMAKE_CURRENT_LIST_DIR}/libossia/3rdparty/r8brain-free-src/r8bbase.cpp"
)

if(NOT MSVC)
  if(CMAKE_BUILD_TYPE MATCHES ".*Deb.*")
    target_compile_options(r8brain PRIVATE -O3 -march=native)
  endif()
endif()

target_include_directories(
  r8brain
  PUBLIC
     "${CMAKE_CURRENT_LIST_DIR}/libossia/3rdparty/r8brain-free-src/"
)
