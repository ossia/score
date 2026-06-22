add_library(opengametools INTERFACE IMPORTED GLOBAL)
target_include_directories(opengametools SYSTEM INTERFACE "${CMAKE_CURRENT_LIST_DIR}/opengametools/src")
