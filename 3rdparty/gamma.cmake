add_library(gamma STATIC
  "${CMAKE_CURRENT_LIST_DIR}/Gamma/src/Conversion.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Gamma/src/DFT.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Gamma/src/Domain.cpp"
  )

target_include_directories(
  gamma
  PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/Gamma"
)
