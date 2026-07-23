score_use_system(use_sys Gamma)
if(use_sys)
  find_package(Gamma GLOBAL QUIET)
  if(NOT TARGET gamma)
    find_library(GAMMA_LIBRARY NAMES Gamma gamma)
    find_path(GAMMA_INCLUDE_DIR Gamma/Gamma.h)
    if(GAMMA_LIBRARY AND GAMMA_INCLUDE_DIR)
      add_library(gamma INTERFACE IMPORTED GLOBAL)
      target_include_directories(gamma SYSTEM INTERFACE "${GAMMA_INCLUDE_DIR}")
      target_link_libraries(gamma INTERFACE "${GAMMA_LIBRARY}")
    endif()
  endif()
endif()

if(NOT TARGET gamma)
add_library(gamma STATIC
  "${CMAKE_CURRENT_LIST_DIR}/Gamma/src/Conversion.cpp"
  # "${CMAKE_CURRENT_LIST_DIR}/Gamma/src/DFT.cpp"
  # "${CMAKE_CURRENT_LIST_DIR}/Gamma/src/FFT.cpp"
  # "${CMAKE_CURRENT_LIST_DIR}/Gamma/src/fftpack++1.cpp"
  # "${CMAKE_CURRENT_LIST_DIR}/Gamma/src/fftpack++2.cpp"
  # "${CMAKE_CURRENT_LIST_DIR}/Gamma/src/Domain.cpp"
  )

target_include_directories(
  gamma
  SYSTEM
  PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/Gamma"
)
endif()
