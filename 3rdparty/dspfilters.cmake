score_use_system(use_sys DSPFilters)
if(use_sys)
  find_package(DSPFilters GLOBAL QUIET)
  if(NOT TARGET dspfilters)
    find_library(DSPFILTERS_LIBRARY NAMES DSPFilters dspfilters)
    find_path(DSPFILTERS_INCLUDE_DIR DspFilters/Dsp.h)
    if(DSPFILTERS_LIBRARY AND DSPFILTERS_INCLUDE_DIR)
      add_library(dspfilters INTERFACE IMPORTED GLOBAL)
      target_include_directories(dspfilters SYSTEM INTERFACE "${DSPFILTERS_INCLUDE_DIR}")
      target_link_libraries(dspfilters INTERFACE "${DSPFILTERS_LIBRARY}")
    endif()
  endif()
endif()

if(NOT TARGET dspfilters)
add_library(dspfilters
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
  if(CMAKE_BUILD_TYPE MATCHES ".*Debug.*")
    target_compile_options(dspfilters PRIVATE -O3 -march=native)
  endif()
  target_compile_options(dspfilters PRIVATE -w)
endif()

target_include_directories(
  dspfilters
  SYSTEM PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/DSPFilters/DSPFilters/include"
)
endif()
