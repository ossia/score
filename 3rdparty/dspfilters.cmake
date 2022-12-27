add_library(dspfilters STATIC
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Bessel.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Biquad.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Butterworth.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Cascade.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/ChebyshevI.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/ChebyshevII.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Custom.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Design.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Documentation.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Elliptic.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Filter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Legendre.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/Param.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/PoleFilter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/RBJ.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/RootFinder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/source/State.cpp"
)

if(NOT MSVC AND NOT CMAKE_CROSSCOMPILING)
  if(CMAKE_BUILD_TYPE MATCHES ".*Deb.*")
    target_compile_options(dspfilters PRIVATE -O3 -march=native)
  endif()
endif()

target_include_directories(
  dspfilters
  PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/include"
)
