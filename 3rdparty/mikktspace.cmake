add_library(mikktspace STATIC "${CMAKE_CURRENT_LIST_DIR}/mikktspace/mikktspace.c")
target_include_directories(mikktspace SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}/mikktspace")
set_target_properties(mikktspace PROPERTIES UNITY_BUILD 0)
