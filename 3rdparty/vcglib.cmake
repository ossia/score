add_library(vcglib INTERFACE IMPORTED GLOBAL)
target_include_directories(vcglib SYSTEM INTERFACE "${CMAKE_CURRENT_LIST_DIR}/vcglib")
