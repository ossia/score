add_library(miniply STATIC "${CMAKE_CURRENT_LIST_DIR}/miniply/miniply.cpp")
target_include_directories(miniply SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}/miniply")
set_target_properties(miniply PROPERTIES UNITY_BUILD 0)
